// Actual RunMotionConfig -> Run -> RunArcade -> RunMotion, guest-free.
// MODE(stock|raw) SCENARIO(success|cancel|freeze-fail) FRESH_ABSOLUTE_ROOT
// [PINNED_TITLE_BMP]. One process per fresh root; preserve every output.
// Compile the motion/config/raw/census/scout/update branches as for admission.
// Controller lifecycle is stubbed; this NEVER calls Begin/Install, a Runtime,
// GPU, window, game, OS input or MAIN. It is not a configuration/scene proof.
// Title input is read-only. Other pixels/provenance are explicitly synthetic.
// Full paths use the real bounded waits, seven button pulses and one RT pulse.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#include <cstdlib>
#include <vector>

#if !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || \
    !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || \
    !defined(MCLA_UPDATE_TIMING_PROBE)
#error Compile every configuration/census/stationary admission branch explicitly.
#endif
namespace arcade_integration_stub { unsigned calls{}; }
namespace mcla::input_return {
bool Enabled() { ++arcade_integration_stub::calls; return false; }
void Enable() { ++arcade_integration_stub::calls; throw std::runtime_error("no census enable"); }
void Install(rex::Runtime&) { ++arcade_integration_stub::calls; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++arcade_integration_stub::calls; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++arcade_integration_stub::calls; throw std::runtime_error("no census cut"); }
void Disarm() noexcept { ++arcade_integration_stub::calls; }
}
namespace {
using namespace std::chrono_literals;
uint64_t NowMs() {
  return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
void Keys(const Json& object, const std::set<std::string>& expected) {
  Need(object.is_object(), "exact JSON object");
  std::set<std::string> actual;
  for (const auto& [key, value] : object.items()) actual.insert(key);
  Need(actual == expected, "exact JSON fields");
}
class ArcadePresenter final : public rex::ui::Presenter {
 public:
  ArcadePresenter() : Presenter({}) {}
  rex::ui::RawImage title;
  unsigned calls{};
  rex::ui::Surface::TypeFlags GetSupportedSurfaceTypes() const override { return 0; }
  bool CaptureGuestOutput(rex::ui::RawImage&) override { throw std::runtime_error("wrong capture API"); }
  rex::ui::GuestCaptureStatus CaptureGuestOutputWithProvenance(rex::ui::GuestOutputCapture& out) override {
    ++calls;
    if (title.data.empty() || calls > 43) throw std::runtime_error("unexpected capture");
    out = {};
    if (calls <= 3) out.image = title;
    else {
      out.image.width = 1280; out.image.height = 720; out.image.stride = 5120;
      out.image.data.resize(size_t(1280) * 720 * 4);
      for (size_t i = 0; i < out.image.data.size(); i += 4) {
        out.image.data[i] = uint8_t(calls); out.image.data[i+1] = 37;
        out.image.data[i+2] = 83; out.image.data[i+3] = 255;
      }
    }
    auto& p = out.provenance;
    p.presenter_epoch = 1; p.acquired_sequence = calls; p.qpc_frequency = 10000000;
    p.refresh_cpu_begin_qpc = p.refresh_cpu_end_qpc = p.acquire_before_qpc =
        p.acquire_after_qpc = p.copy_submit_before_qpc = p.copy_completion_observed_qpc =
        p.pixels_ready_qpc = 100 + calls;
    return rex::ui::GuestCaptureStatus::kSuccess;
  }
 protected:
  SurfacePaintConnectResult ConnectOrReconnectPaintingToSurfaceFromUIThread(
      rex::ui::Surface&, uint32_t, uint32_t, bool, bool&) override { throw std::runtime_error("no surface"); }
  void DisconnectPaintingFromSurfaceFromUIThreadImpl() override {}
  bool RefreshGuestOutputImpl(uint32_t, uint32_t, uint32_t,
      std::function<bool(GuestOutputRefreshContext&)>, bool&) override { throw std::runtime_error("no refresh"); }
  PaintResult PaintAndPresentImpl(bool) override { throw std::runtime_error("no present"); }
};
void VerifyImage(const std::filesystem::path& file, const Json& p, unsigned sequence,
                 const rex::ui::RawImage& title) {
  Keys(p, {"width", "height", "stride", "presenter_epoch", "acquired_sequence", "qpc_frequency",
           "refresh_cpu_begin_qpc", "refresh_cpu_end_qpc", "acquire_before_qpc", "acquire_after_qpc",
           "copy_submit_before_qpc", "copy_completion_observed_qpc", "pixels_ready_qpc"});
  Need(p.at("width") == 1280 && p.at("height") == 720 && p.at("stride") == 5120 &&
       p.at("presenter_epoch") == 1 && p.at("acquired_sequence") == sequence &&
       p.at("qpc_frequency") == 10000000, "synthetic source identity/layout exact");
  for (const auto* key : {"refresh_cpu_begin_qpc", "refresh_cpu_end_qpc", "acquire_before_qpc",
       "acquire_after_qpc", "copy_submit_before_qpc", "copy_completion_observed_qpc", "pixels_ready_qpc"})
    Need(p.at(key) == 100 + sequence, "exact synthetic timestamps, not real configuration binding");
  const auto bytes = Read(file, 4 * 1024 * 1024);
  auto le = [&](size_t offset, unsigned count) {
    uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i)
      value |= uint32_t(static_cast<unsigned char>(bytes.at(offset+i))) << (8*i);
    return value;
  };
  constexpr size_t payload = size_t(1280) * 720 * 3;
  Need(bytes.size() == 54 + payload && le(0,2) == 0x4d42 && le(2,4) == bytes.size() &&
       le(10,4) == 54 && le(14,4) == 40 && le(18,4) == 1280 && le(22,4) == 720 &&
       le(26,2) == 1 && le(28,2) == 24 && le(30,4) == 0 && le(34,4) == payload,
       "exact actual bottom-up 720p BMP transport");
  for (uint32_t y = 0; y < 720; ++y) for (uint32_t x = 0; x < 1280; ++x) {
    const size_t at = 54 + (size_t(719-y) * 1280 + x) * 3;
    const size_t rgba = size_t(y) * 5120 + x * 4;
    const uint8_t red = sequence == 3 ? title.data.at(rgba) : uint8_t(sequence);
    const uint8_t green = sequence == 3 ? title.data.at(rgba+1) : 37;
    const uint8_t blue = sequence == 3 ? title.data.at(rgba+2) : 83;
    if (uint8_t(bytes[at]) != blue || uint8_t(bytes[at+1]) != green || uint8_t(bytes[at+2]) != red)
      throw std::runtime_error("retained BMP payload differs from exact synthetic source");
  }
  Need(true, "every saved pixel verified");
}
void Inventory(const std::filesystem::path& root, const std::set<std::filesystem::path>& expected) {
  std::set<std::filesystem::path> actual;
  for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
    Need(!item.is_symlink(), "no output symlink");
    if (item.is_directory()) continue;
    Need(item.is_regular_file(), "regular output artifact only");
    actual.insert(item.path().lexically_relative(root));
  }
  Need(actual == expected, "exact retained artifact inventory with no extra files");
}
void VerifyComplete(const std::filesystem::path& root, const ArcadePresenter& presenter,
                    std::set<std::filesystem::path>& expected) {
  const auto nav = Json::parse(Read(root / "autostart" / "result.json"));
  const auto arcade = Json::parse(Read(root / "arcade-navigation" / "result.json"));
  const auto motion = Json::parse(Read(root / "motion-calibration" / "result.json"));
  for (const auto* object : {&nav, &arcade, &motion}) Need(object->at("token") == stub::token, "shared stage token");
  Need(nav.at("schema") == "mcla-autostart-navigation-v1" && nav.at("frames").size() == 6 &&
       nav.at("status") == "INPUT_COMPLETE_SCENE_REVIEW_PENDING" && nav.at("title_attempts") == 3 &&
       nav.at("title_ready") == true && nav.at("down_reads").get<uint64_t>() > 0,
       "actual title/START and six stored navigation sources");
  Need(nav.at("neutral_ack_ms").get<uint64_t>() >= nav.at("released_ms").get<uint64_t>() &&
       nav.at("released_ms").get<uint64_t>() >= nav.at("published_ms").get<uint64_t>() + 5000,
       "real START duration and native neutral acknowledgement");
  for (unsigned i = 0; i < 6; ++i) {
    const auto file = std::filesystem::path("autostart") / (i ? "post-" + std::to_string(i) + ".bmp" : "title.bmp");
    expected.insert(file); VerifyImage(root / file, nav.at("frames")[i], i + 3, presenter.title);
  }
  Need(arcade.at("schema") == "mcla-arcade-navigation-v1" &&
       arcade.at("status") == "ARCADE_NAVIGATION_COLLECTED_REVIEW_PENDING" &&
       arcade.at("failure") == "" && arcade.at("capture_retry_count") == 0 &&
       arcade.at("commands").size() == 6 && arcade.at("frames").size() == 10,
       "actual RunArcade closes ten-image six-command receipt");
  uint64_t previous_neutral = nav.at("neutral_ack_ms").get<uint64_t>();
  for (unsigned i = 0; i < 6; ++i) {
    const auto& command = arcade.at("commands")[i];
    const auto publication = command.at("published_ms").get<uint64_t>();
    Need(command.at("sequence") == i + 1 &&
         command.at("status") == static_cast<int>(mcla::arcade_input::Status::Succeeded) &&
         command.at("error") == static_cast<int>(mcla::arcade_input::Error::None) &&
         command.at("released_clock_valid") == true &&
         publication >= previous_neutral &&
         command.at("release_deadline_ms") == publication + 200 &&
         command.at("neutral_ack_deadline_ms") == publication + 5200 &&
         command.at("down_ack_ms").get<uint64_t>() >= publication &&
         command.at("released_ms").get<uint64_t>() >= publication + 200 &&
         command.at("neutral_ack_ms").get<uint64_t>() >= command.at("released_ms").get<uint64_t>(),
         "all six menu down/release/neutral ACKs retained");
    previous_neutral = command.at("neutral_ack_ms").get<uint64_t>();
  }
  Need(arcade.at("terminal_menu") == arcade.at("commands")[5], "terminal menu is sixth completed command");
  for (unsigned i = 0; i < 10; ++i) {
    const auto file = std::filesystem::path("arcade-navigation") / ("frame-" + std::to_string(i) + ".bmp");
    expected.insert(file); VerifyImage(root / file, arcade.at("frames")[i].at("provenance"), i + 9, presenter.title);
  }
  Need(motion.at("schema") == "mcla-arcade-motion-v1" && motion.at("frames").size() == 25 &&
       motion.at("status") == "MOTION_CALIBRATION_COLLECTED_REVIEW_PENDING" && motion.at("failure") == "" &&
       motion.at("right_trigger") == 255 &&
       motion.at("input_policy") == "exclusive_internal_rt255_3000ms_after_arcade_neutral" &&
       motion.at("receipt_scope") == "motion_only_after_arcade" &&
       motion.at("capture_retry_count") == 0 && motion.at("capture_offsets_ms").size() == 24 &&
       motion.at("down_read_count").get<uint64_t>() > 0 && motion.at("neutral_read_count").get<uint64_t>() > 0 &&
       motion.at("poll_count").get<uint64_t>() >= 27,
       "dense RT255 motion collection after actual closed Arcade receipt");
  const auto published = motion.at("published_ms").get<uint64_t>();
  Need(previous_neutral <= motion.at("frames")[0].at("request_before_ms").get<uint64_t>() &&
       motion.at("release_deadline_ms") == published + 3000 &&
       motion.at("neutral_ack_deadline_ms") == published + 8000 &&
       motion.at("down_ack_ms").get<uint64_t>() >= published &&
       motion.at("released_ms").get<uint64_t>() >= published + 3000 &&
       motion.at("neutral_ack_ms").get<uint64_t>() >= motion.at("released_ms").get<uint64_t>(),
       "actual motion publication/hold/release/neutral after menu");
  for (unsigned i = 0; i < 25; ++i) {
    const auto& frame = motion.at("frames")[i];
    Need(frame.at("slot") == i && frame.at("saved") == true &&
         frame.at("capture_status") == static_cast<int>(rex::ui::GuestCaptureStatus::kSuccess),
         "exact successful motion slot");
    if (!i) Need(frame.at("due_ms").is_null(), "before frame has no invented due time");
    else Need(motion.at("capture_offsets_ms")[i-1] == i * 250 &&
              frame.at("due_ms") == published + i * 250 &&
              frame.at("request_before_ms").get<uint64_t>() >= published + i * 250 &&
              frame.at("request_before_ms").get<uint64_t>() <= published + i * 250 + 250,
              "exact predeclared dense offsets and request bounds");
    const auto file = std::filesystem::path("motion-calibration") / (i ? "motion-" + std::to_string(i) + ".bmp" : "before.bmp");
    expected.insert(file); VerifyImage(root / file, frame.at("provenance"), i + 19, presenter.title);
  }
  Need(nav.at("scene_proven") == false && nav.at("fps_proven") == false, "navigation no semantic proof");
  for (const auto* key : {"scene_proven", "race_ready_proven", "physical_speed_proven", "fps_proven", "os_input", "desktop_capture"})
    Need(arcade.at(key) == false, "Arcade no semantic proof");
  for (const auto* key : {"race_ready_proven", "os_input", "desktop_capture", "scene_proven", "movement_proven",
                         "physical_speed_proven", "fps_proven", "physical_hold_duration_proven", "cleanup_proven"})
    Need(motion.at(key) == false, "motion no semantic proof");
}
}
int main(int argc, char** argv) {
  std::jthread watchdog([](std::stop_token stop) {
    const auto begin = NowMs();
    while (!stop.stop_requested() && NowMs() - begin < 150000) std::this_thread::sleep_for(20ms);
    if (!stop.stop_requested()) std::_Exit(4);
  });
  int exit_code = 1;
  try {
    Need(argc == 4 || argc == 5, "MODE SCENARIO FRESH_ABSOLUTE_ROOT [PINNED_TITLE_BMP]");
    const std::string mode = argv[1], scenario = argv[2];
    Need(mode == "stock" || mode == "raw", "exact configuration mode");
    Need(scenario == "success" || scenario == "cancel" || scenario == "freeze-fail", "exact scenario");
    const bool full = scenario != "cancel", cut_ok = scenario != "freeze-fail", success = scenario == "success";
    Need((argc == 5) == full, "title required only for complete capture paths");
    const std::filesystem::path root = argv[3];
    Need(root.is_absolute() && root == root.lexically_normal() && !root.filename().empty() &&
         !std::filesystem::exists(root), "fresh absolute owned output only");
    auto parent = mcla::launcher::fs::OpenDirectory(root.parent_path().wstring());
    Need(parent.has_value(), "safe existing parent held throughout");
    Need(std::filesystem::create_directory(root), "create only fresh owned root");
    Need(rex::cvar::SetFlagByName("mcla_arcade_config_probe", "true"), "enable explicit new mode");
    REXCVAR_SET(mcla_autostart_probe, true); REXCVAR_SET(mcla_autostart_token, stub::token);
    REXCVAR_SET(mcla_arcade_navigation_probe, true); REXCVAR_SET(mcla_motion_calibration_probe, true);
    REXCVAR_SET(mcla_motion_config_probe, true); REXCVAR_SET(mcla_timer_config_probe, true);
    REXCVAR_SET(mcla_raw_config_experiment, mode == "raw"); REXCVAR_SET(mcla_fixed60_config_experiment, false);
    REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
    Need(Admits(), "actual new admission before synthetic workflow");
    stub::fail_freeze = !cut_ok;
    a::Driver driver; Need(driver.Setup() == rex::X_STATUS{0}, "actual driver setup");
    ArcadePresenter presenter; std::stop_source stop;
    if (full) {
      NoCapture loader; LoadTitle(loader, std::filesystem::path(argv[4]));
      presenter.title = std::move(loader.synthetic_title);
    } else stop.request_stop();
    std::atomic<bool> reader_failed{false};
    std::vector<uint16_t> buttons;
    unsigned motion_edges = 0;
    std::jthread reader;
    if (full) reader = std::jthread([&](std::stop_token token) {
      uint16_t prior_button = 0; bool prior_motion = false;
      while (!token.stop_requested()) {
        rex::input::X_INPUT_STATE state{};
        const auto result = driver.GetState(0, &state);
        const auto& pad = state.gamepad;
        if (result != rex::X_RESULT{0} || (pad.buttons && pad.buttons != 0x10 && pad.buttons != 0x200 && pad.buttons != 0x1000) ||
            (pad.right_trigger && pad.right_trigger != 255) || (pad.buttons && pad.right_trigger) ||
            pad.left_trigger || pad.thumb_lx || pad.thumb_ly || pad.thumb_rx || pad.thumb_ry) {
          reader_failed.store(true); stop.request_stop(); return;
        }
        if (pad.buttons && !prior_button) buttons.push_back(pad.buttons);
        if (pad.right_trigger && !prior_motion) ++motion_edges;
        prior_button = pad.buttons; prior_motion = pad.right_trigger != 0;
        std::this_thread::sleep_for(2ms);
      }
    });
    const auto begin = NowMs();
    const bool returned = a::RunMotionConfig(stop.get_token(), presenter, driver, root);
    const auto elapsed = NowMs() - begin;
    if (reader.joinable()) { reader.request_stop(); reader.join(); }
    Need(!reader_failed.load() && returned == success, "exact full workflow outcome");
    Need(stub::freezes == 1 && stub::disarms == 1 && !stub::installs && !stub::begins &&
         !arcade_integration_stub::calls, "one stub cut/disarm; no Runtime/controller begin/census");
    Need(presenter.calls == (full ? 43u : 0u) && motion_edges == unsigned(full), "exact actual capture/RT edge count");
    Need(buttons == (full ? std::vector<uint16_t>{0x10,0x10,0x200,0x1000,0x1000,0x1000,0x1000}
                         : std::vector<uint16_t>{}), "one START and six exact menu down edges");
    Need(elapsed < 140000 && (!full || elapsed >= 100000), "real bounded waits, no synthetic clock acceleration");
    rex::input::X_INPUT_STATE neutral{};
    Need(driver.GetState(0, &neutral) == rex::X_RESULT{0} && !neutral.gamepad.buttons &&
         !neutral.gamepad.right_trigger && !driver.ArmMotion(), "terminal neutral and no rearm");
    const auto terminal = Json::parse(Read(root / "motion-config" / "result.json"));
    Keys(terminal, {"schema", "token", "mode", "status", "failure", "stop_requested", "navigation_collected",
         "motion_collected", "configuration_written", "configuration_file", "configuration_bytes",
         "controller_disarmed", "injection_requested", "input_injected", "os_input", "desktop_capture",
         "scene_proof", "movement_proof", "physical_speed_proof", "fps_proof", "configuration_accepted"});
    Need(terminal.at("schema") == "mcla-arcade-config-terminal-v1" && terminal.at("token") == stub::token &&
         terminal.at("mode") == mode && terminal.at("status") == (success ? "COLLECTED_REVIEW_PENDING" : "FAILED") &&
         terminal.at("failure") == (success ? "" : cut_ok ? "NAVIGATION_FAILED" : "CONFIGURATION_CUT_OR_WRITE_FAILED") &&
         terminal.at("stop_requested") == !full && terminal.at("navigation_collected") == full &&
         terminal.at("motion_collected") == full && terminal.at("controller_disarmed") == true &&
         terminal.at("injection_requested") == true && terminal.at("input_injected") == full,
         "new exact terminal retains honest stage outcomes");
    for (const auto* key : {"os_input", "desktop_capture", "scene_proof", "movement_proof",
                           "physical_speed_proof", "fps_proof", "configuration_accepted"})
      Need(terminal.at(key).is_boolean() && terminal.at(key) == false, "terminal all proof flags false");
    const auto config = Read(root / "motion-config" / "timer-config.json");
    Need(terminal.at("configuration_file") == "timer-config.json" &&
         terminal.at("configuration_written") == cut_ok && terminal.at("configuration_bytes") == config.size() &&
         config == (cut_ok ? stub::retained : ""), "exact unmodified stub cut bytes, or retained empty failed cut");
    std::set<std::filesystem::path> expected{
        "motion-config/result.json", "motion-config/timer-config.json", "autostart/result.json"};
    if (full) {
      expected.insert("arcade-navigation/result.json"); expected.insert("motion-calibration/result.json");
      VerifyComplete(root, presenter, expected);
      Need(expected.size() == 46, "41 actual BMP files and five receipts");
    } else {
      const auto nav = Json::parse(Read(root / "autostart" / "result.json"));
      Need(nav.at("status") == "FAILED" && nav.at("frames").empty() &&
           !std::filesystem::exists(root / "arcade-navigation") &&
           !std::filesystem::exists(root / "motion-calibration"), "cancel never starts Arcade or motion");
    }
    Inventory(root, expected);
    std::cout << "ARCADE_CONFIG_INTEGRATION mode=" << mode << " scenario=" << scenario
              << " checks=" << checks << " failures=0 captures=" << presenter.calls
              << " saved_bmps=" << (full ? 41 : 0) << " elapsed_ms=" << elapsed
              << " SYNTHETIC_CONTROLLER_AND_PIXELS_ONLY\n";
    std::cout.flush(); std::cerr.flush();
    exit_code = std::cout.good() && std::cerr.good() ? 0 : 2;
  } catch (const std::exception& e) {
    std::cerr << "FAIL " << e.what() << '\n'; std::cerr.flush();
    exit_code = std::cerr.good() ? 1 : 2;
  }
  watchdog.request_stop(); watchdog.join();
  return exit_code;
}
