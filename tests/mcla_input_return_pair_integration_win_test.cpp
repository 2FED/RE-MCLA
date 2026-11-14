// Actual Run/Driver/ValidateConfig with synthetic presenter and census stubs.
// One fresh process/root per OFF or ON mode. No game, Runtime, GPU or OS input.
// Compile/link as input-return integration; include path chooses frozen source.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE)
#error Explicit private census build required.
#endif

namespace pair_stub {
unsigned enables{}, installs{}, begins{}, freezes{}, disarms{};
bool fixed60{};
std::string serialized;
}
namespace mcla::input_return {
bool Enabled() { return false; }
void Enable() { ++pair_stub::enables; throw std::runtime_error("no real census enable in synthetic Run test"); }
void Install(rex::Runtime&) { ++pair_stub::installs; throw std::runtime_error("no Runtime in synthetic Run test"); }
void Begin(const std::string& token, uint64_t duration_ms) {
  ++pair_stub::begins;
  Need(token == stub::token && duration_ms == 30000, "same bounded census token/window");
  Need(REXCVAR_GET(mcla_fixed60_config_experiment) == pair_stub::fixed60,
       "selected intervention mode unchanged when census begins");
}
std::string Freeze(const std::string& token) {
  ++pair_stub::freezes;
  Need(token == stub::token && pair_stub::begins == 1, "same begun census freezes");
  Need(REXCVAR_GET(mcla_fixed60_config_experiment) == pair_stub::fixed60,
       "selected intervention mode unchanged at cut");
  pair_stub::serialized = std::string("{\"synthetic_census\":true,\"fixed60\":") +
      (pair_stub::fixed60 ? "true}" : "false}");
  return pair_stub::serialized;
}
void Disarm() noexcept { ++pair_stub::disarms; }
}

int main(int argc, char** argv) {
  try {
    Need(argc == 4, "OFF_OR_ON FRESH_ABSOLUTE_ROOT PINNED_TITLE required");
    const std::string mode = argv[1];
    Need(mode == "off" || mode == "on", "exact paired mode");
    pair_stub::fixed60 = mode == "on";
    Registry();
    REXCVAR_SET(mcla_fixed60_config_experiment, pair_stub::fixed60);
    REXCVAR_SET(mcla_input_return_probe, true);
    REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
    REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
    Need(Admits(), "selected matched mode passes actual ValidateConfig");
    const std::filesystem::path root(argv[2]);
    Need(root.is_absolute() && !std::filesystem::exists(root), "fresh absolute owned root");
    Need(std::filesystem::create_directory(root), "fresh root created");
    a::Driver driver; NoCapture presenter; std::stop_source stop;
    LoadTitle(presenter, argv[3]);
    std::atomic<bool> reader_failed{false};
    unsigned edges{};
    std::jthread reader([&](std::stop_token done) {
      bool prior{};
      while (!done.stop_requested()) {
        rex::input::X_INPUT_STATE state{};
        if (driver.GetState(0, &state) != rex::X_RESULT{0} ||
            (state.gamepad.buttons != 0 && state.gamepad.buttons != mcla::autostart_input::kStartButton) ||
            state.gamepad.left_trigger != 0 || state.gamepad.right_trigger != 0 ||
            int16_t(state.gamepad.thumb_lx) != 0 || int16_t(state.gamepad.thumb_ly) != 0 ||
            int16_t(state.gamepad.thumb_rx) != 0 || int16_t(state.gamepad.thumb_ry) != 0) {
          reader_failed.store(true); stop.request_stop(); return;
        }
        const bool down = state.gamepad.buttons != 0;
        if (down && !prior) ++edges;
        prior = down;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    });
    const bool returned = a::Run(stop.get_token(), presenter, driver, root);
    reader.request_stop(); reader.join();
    Need(returned && !reader_failed.load() && !stop.stop_requested(), "actual Run succeeds in selected mode");
    Need(edges == 1, "exactly one START pulse and no other inputs");
    Need(!driver.PollMotion().published_ms, "navigation-only fixture never arms RT");
    Need(pair_stub::begins == 1 && pair_stub::freezes == 1 && pair_stub::disarms == 1,
         "single census begin/cut/terminal disarm");
    Need(!pair_stub::enables && !pair_stub::installs && !stub::installs && !stub::begins &&
         !stub::freezes && !stub::disarms, "no real observer/controller lifecycle or Runtime");
    Need(REXCVAR_GET(mcla_fixed60_config_experiment) == pair_stub::fixed60 &&
         REXCVAR_GET(mcla_input_return_probe) && REXCVAR_GET(mcla_autostart_start_delay_ms) == 8000u,
         "mode/census/delay unchanged through Run");
    const auto nav = Json::parse(Read(root / "autostart/result.json"));
    Need(nav.at("schema") == "mcla-autostart-navigation-v2" &&
         nav.at("status") == "INPUT_COMPLETE_SCENE_REVIEW_PENDING" &&
         nav.at("delay_ms") == 8000 && nav.at("input_status") == 3 && nav.at("input_error") == 0,
         "same delayed successful navigation receipt");
    const auto publication = nav.at("published_ms").get<uint64_t>();
    const auto down = nav.at("down_ack_ms").get<uint64_t>();
    const auto released = nav.at("released_ms").get<uint64_t>();
    const auto neutral = nav.at("neutral_ack_ms").get<uint64_t>();
    Need(nav.at("start_due_ms").get<uint64_t>() == nav.at("title_confirmed_ms").get<uint64_t>() + 8000 &&
         nav.at("start_due_ms").get<uint64_t>() <= nav.at("arm_before_ms").get<uint64_t>() &&
         nav.at("arm_before_ms").get<uint64_t>() <= publication &&
         publication <= nav.at("arm_after_ms").get<uint64_t>(), "same actual delayed publication bracket");
    Need(publication <= down && down < publication + 5000 && publication + 5000 <= released &&
         released <= neutral && neutral < publication + 10000 && nav.at("down_reads").get<uint64_t>() > 0,
         "same START hold and neutral ACK bounds");
    Need(nav.at("frames").size() == 6 && presenter.calls == 8,
         "same three title observations then five fixed post observations");
    Need(nav.at("scene_proven") == false && nav.at("fps_proven") == false,
         "synthetic success never proves gameplay or FPS");
    unsigned images{};
    for (const auto& entry : std::filesystem::directory_iterator(root / "autostart"))
      if (entry.path().extension() == ".bmp") ++images;
    Need(images == 6, "exact six stored navigation images");
    Need(Read(root / "input-return/result.json") == pair_stub::serialized, "exact selected-mode synthetic census bytes");
    std::cout << "INPUT_RETURN_PAIR_INTEGRATION checks=" << checks << " failures=0 mode=" << mode << '\n';
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL " << e.what() << '\n'; return 1;
  }
}
