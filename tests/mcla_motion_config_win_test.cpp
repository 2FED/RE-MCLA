// Guest-free test of actual autostart admission, file ownership and terminal
// wrapper. Controller stubs are deliberate: no wrapper/Apply/runtime proof.
// Compile AUTOSTART, MOTION_CALIBRATION, MOTION_CONFIG, TIMER_CONFIG; optionally
// UPDATE_TIMING and GUEST_SOURCE_SCOUT to exercise the compiled admission branch.
// Include autostart implementation once, link launcher_fs_win.cpp + matching SDK.
// One process per MODE FRESH_ABSOLUTE_ROOT [PINNED_TITLE_BMP]; never reuse/delete
// an output root. Success uses a read-only staged title bitmap and real waits,
// but every presenter publication is synthetic (not a game/scene proof).
#include "mcla_autostart_win.cpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <set>

#if !defined(MCLA_MOTION_CONFIG_PROBE)
#error This test requires the new explicit standalone mode.
#endif
REXCVAR_DEFINE_BOOL(mcla_timer_config_probe, false, "Test", "Stub controller enable");
REXCVAR_DEFINE_BOOL(mcla_fixed60_config_experiment, false, "Test", "Stub fixed60 enable");
REXCVAR_DEFINE_BOOL(mcla_raw_config_experiment, false, "Test", "Forbidden raw control");
REXCVAR_DEFINE_BOOL(mcla_source_scout_probe, false, "Test", "Forbidden stationary scout");
REXCVAR_DEFINE_BOOL(mcla_update_timing_probe, false, "Test", "Forbidden timing observer");
REXCVAR_DEFINE_BOOL(mcla_motion_unknown_probe, false, "Test", "Unknown competing probe");
REXCVAR_DEFINE_BOOL(mcla_motion_unknown_experiment, false, "Test", "Unknown competing experiment");
#if defined(MCLA_GUEST_SOURCE_SCOUT_PROBE)
REXCVAR_DEFINE_STRING(mcla_source_scout_token, "", "Test", "Unused stationary token");
namespace mcla::guest_source_scout {
bool Enabled() { return REXCVAR_GET(mcla_source_scout_probe); }
void ValidateExclusiveConfig(std::string_view) { throw std::runtime_error("stationary validator must not run"); }
}
#endif
namespace stub {
unsigned freezes{}, disarms{}, installs{}, begins{};
bool fail_freeze{};
constexpr const char* token = "e69a68cd28e641f5a1e8e92d720f1068";
std::string retained;
}
namespace mcla::timer_config {
bool Enabled() { return REXCVAR_GET(mcla_timer_config_probe); }
bool ExperimentEnabled() { return REXCVAR_GET(mcla_fixed60_config_experiment); }
bool RawExperimentEnabled() { return REXCVAR_GET(mcla_raw_config_experiment); }
void ValidateExperimentSelection() {
  if (ExperimentEnabled() && RawExperimentEnabled()) throw std::runtime_error("mutually exclusive");
}
void Install(rex::Runtime&) { ++stub::installs; throw std::runtime_error("no runtime installation permitted"); }
void Begin(uint64_t, uint64_t, uint64_t, const std::string&) {
  ++stub::begins; throw std::runtime_error("no runtime begin permitted");
}
std::string Freeze(const std::string& token) {
  ++stub::freezes;
  if (stub::fail_freeze || token != stub::token) throw std::runtime_error("synthetic cut failure");
  stub::retained = std::string("{\"synthetic_controller_fixture\":true,\"token\":\"") + token + "\"}";
  return stub::retained;
}
void Disarm() noexcept { ++stub::disarms; }
}
namespace {
namespace a = mcla::autostart;
using Json = nlohmann::json;
unsigned checks{};
void Need(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
bool Admits() { try { a::ValidateConfig(); return true; } catch (...) { return false; } }
void Registry() {
  Need(!a::MotionConfigEnabled() && Admits(), "new mode defaults OFF");
  const auto* delay = rex::cvar::GetFlagInfo("mcla_autostart_start_delay_ms");
  Need(delay && delay->getter && delay->setter, "START delay flag exists");
  Need(delay->default_value == "0" && delay->getter() == "0" &&
       delay->lifecycle == rex::cvar::Lifecycle::kInitOnly, "START delay defaults zero and is init-only");
  Need(delay->setter("8000") && !Admits(), "delay requires enabled fixed60 motion/config");
  Need(delay->setter("0") && Admits(), "zero delay retains default-off admission");
  REXCVAR_SET(mcla_motion_config_probe, true);
  Need(!Admits(), "missing dependencies reject");
  REXCVAR_SET(mcla_autostart_probe, true);
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_timer_config_probe, true);
  Need(!Admits(), "missing token rejects");
  REXCVAR_SET(mcla_autostart_token, stub::token);
  Need(Admits(), "stock controller standalone admitted");
  Need(delay->setter("8000") && !Admits(), "stock configuration rejects delayed START");
  REXCVAR_SET(mcla_fixed60_config_experiment, true);
  Need(Admits(), "fixed60 controller standalone admitted");
  for (const char* invalid : {"1", "7999", "8001", "4294967295"})
    Need(delay->setter(invalid) && !Admits(), "only exact zero or eight-second delay admitted");
  Need(delay->setter("8000") && Admits(), "eight-second delay restored");
  REXCVAR_SET(mcla_source_scout_probe, true);
  Need(!Admits(), "delayed START never admits scout");
  REXCVAR_SET(mcla_source_scout_probe, false);
  Need(delay->setter("0") && Admits(), "old zero-delay path restored");
  REXCVAR_SET(mcla_autostart_token, "NOT-A-TOKEN");
  Need(!Admits(), "invalid token rejects");
  REXCVAR_SET(mcla_autostart_token, stub::token);
  REXCVAR_SET(mcla_timer_config_probe, false);
  Need(!Admits(), "missing controller rejects");
  REXCVAR_SET(mcla_timer_config_probe, true);
  REXCVAR_SET(mcla_raw_config_experiment, true);
  Need(!Admits(), "raw and fixed mixed reject");
  REXCVAR_SET(mcla_fixed60_config_experiment, false);
  Need(!Admits(), "raw alone rejects");
  REXCVAR_SET(mcla_raw_config_experiment, false);
  REXCVAR_SET(mcla_source_scout_probe, true);
  Need(!Admits(), "stationary scout rejects");
  REXCVAR_SET(mcla_source_scout_probe, false);
  REXCVAR_SET(mcla_update_timing_probe, true);
  Need(!Admits(), "fake enabled observer rejects");
  REXCVAR_SET(mcla_update_timing_probe, false);
  REXCVAR_SET(mcla_motion_unknown_probe, true);
  Need(!Admits(), "unknown probe rejects");
  REXCVAR_SET(mcla_motion_unknown_probe, false);
  REXCVAR_SET(mcla_motion_unknown_experiment, true);
  Need(!Admits(), "unknown experiment rejects");
  REXCVAR_SET(mcla_motion_unknown_experiment, false);
  REXCVAR_SET(mcla_motion_config_probe, false);
  Need(!Admits(), "old motion mode still forbids controller");
  REXCVAR_SET(mcla_motion_config_probe, true);
  Need(Admits(), "complete new mode restored");
  Need(!stub::installs && !stub::begins && !stub::freezes && !stub::disarms,
       "admission has no controller lifecycle side effects");
}
class NoCapture final : public rex::ui::Presenter {
 public:
  NoCapture() : Presenter({}) {}
  unsigned calls{};
  std::atomic<bool> third_capture_returned{false};
  rex::ui::RawImage synthetic_title;
  rex::ui::Surface::TypeFlags GetSupportedSurfaceTypes() const override { return 0; }
  bool CaptureGuestOutput(rex::ui::RawImage&) override { ++calls; throw std::runtime_error("unexpected capture"); }
  rex::ui::GuestCaptureStatus CaptureGuestOutputWithProvenance(rex::ui::GuestOutputCapture& out) override {
    ++calls;
    if (synthetic_title.data.empty()) throw std::runtime_error("unexpected capture");
    out = {}; out.image = synthetic_title;
    auto& p = out.provenance;
    p.presenter_epoch = 1; p.acquired_sequence = calls; p.qpc_frequency = 10000000;
    p.refresh_cpu_begin_qpc = p.refresh_cpu_end_qpc = p.acquire_before_qpc = p.acquire_after_qpc =
        p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = 100 + calls;
    if (calls == 3) third_capture_returned.store(true);
    return rex::ui::GuestCaptureStatus::kSuccess;
  }
 protected:
  SurfacePaintConnectResult ConnectOrReconnectPaintingToSurfaceFromUIThread(
      rex::ui::Surface&,uint32_t,uint32_t,bool,bool&) override { throw std::runtime_error("no surface"); }
  void DisconnectPaintingFromSurfaceFromUIThreadImpl() override {}
  bool RefreshGuestOutputImpl(uint32_t,uint32_t,uint32_t,
      std::function<bool(GuestOutputRefreshContext&)>,bool&) override { throw std::runtime_error("no refresh"); }
  PaintResult PaintAndPresentImpl(bool) override { throw std::runtime_error("no present"); }
};
std::string Read(const std::filesystem::path& path, uint64_t maximum = 65536) {
  const auto size = std::filesystem::file_size(path);
  Need(size <= maximum, "bounded test input/output");
  std::ifstream file(path, std::ios::binary);
  std::string result(size, '\0'); file.read(result.data(), static_cast<std::streamsize>(size));
  Need(file.good() || (file.eof() && size_t(file.gcount()) == size), "complete test output");
  return result;
}
void LoadTitle(NoCapture& presenter, const std::filesystem::path& path) {
  Need(path.is_absolute() && std::filesystem::is_regular_file(path), "explicit read-only staged title");
  const auto bytes = Read(path, 4 * 1024 * 1024);
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
       "exact production bottom-up 720p BMP transport");
  auto& image = presenter.synthetic_title;
  image.width = 1280; image.height = 720; image.stride = 5120;
  image.data.resize(size_t(image.stride) * image.height);
  for (uint32_t y = 0; y < 720; ++y) for (uint32_t x = 0; x < 1280; ++x) {
    const size_t source = 54 + (size_t(719-y)*1280+x)*3;
    const size_t target = size_t(y)*image.stride + size_t(x)*4;
    for (unsigned c = 0; c < 3; ++c) image.data[target+c] = uint8_t(bytes[source+2-c]);
    image.data[target+3] = 255;
  }
  Need(mcla_stationary_title::Classify(image) == mcla_stationary_title::Frame::Title,
       "actual production title classifier accepts staged fixture");
}
}
int main(int argc, char** argv) {
  try {
    Need(argc == 3 || argc == 4, "MODE FRESH_ABSOLUTE_ROOT [PINNED_TITLE_BMP] required");
    const std::string mode = argv[1];
    const bool delayed = mode == "delayed-success" || mode == "delayed-cancel";
    const bool success = mode == "success" || mode == "delayed-success";
    const bool captures = success || mode == "delayed-cancel";
    Need((argc == 4) == captures, "title bitmap argument required only for capture modes");
    Need(mode == "cancel" || mode == "cancel-after-input" || mode == "freeze-fail" ||
        mode == "output-exists" || mode == "root-missing" || mode == "nav-output-exists" || captures, "known mode");
    const std::filesystem::path root = std::filesystem::absolute(argv[2]);
    Need(root == std::filesystem::path(argv[2]) && !std::filesystem::exists(root), "fresh absolute root");
    Registry();
    if (mode != "root-missing") Need(std::filesystem::create_directory(root), "create only fresh owned root");
    if (mode == "output-exists") Need(std::filesystem::create_directory(root / "motion-config"), "owned collision fixture");
    if (mode == "nav-output-exists") Need(std::filesystem::create_directory(root / "autostart"), "owned navigation collision fixture");
    stub::fail_freeze = mode == "freeze-fail";
    a::Driver driver; NoCapture presenter; std::stop_source stop;
    if (!captures) stop.request_stop();
    else {
      REXCVAR_SET(mcla_fixed60_config_experiment, true);
      REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
      Need(rex::cvar::SetFlagByName("mcla_autostart_start_delay_ms", delayed ? "8000" : "0"), "select delay control");
      LoadTitle(presenter, std::filesystem::path(argv[3]));
    }
    std::atomic<bool> reader_failed{false};
    unsigned start_edges = 0, motion_edges = 0;
    std::jthread reader;
    if (captures) reader = std::jthread([&](std::stop_token token) {
      std::optional<std::chrono::steady_clock::time_point> cancel_due;
      bool start_down = false, motion_down = false;
      while (!token.stop_requested()) {
        rex::input::X_INPUT_STATE state{};
        if (driver.GetState(0, &state) != rex::X_RESULT{0} ||
            (state.gamepad.buttons != 0 && state.gamepad.buttons != mcla::autostart_input::kStartButton) ||
            (state.gamepad.right_trigger != 0 && state.gamepad.right_trigger != 255) ||
            (state.gamepad.buttons && state.gamepad.right_trigger) || state.gamepad.left_trigger ||
            state.gamepad.thumb_lx || state.gamepad.thumb_ly || state.gamepad.thumb_rx || state.gamepad.thumb_ry ||
            (mode == "delayed-cancel" && (state.gamepad.buttons || state.gamepad.right_trigger))) {
          reader_failed.store(true); stop.request_stop(); return;
        }
        if (state.gamepad.buttons && !start_down) ++start_edges;
        if (state.gamepad.right_trigger && !motion_down) ++motion_edges;
        start_down = state.gamepad.buttons != 0;
        motion_down = state.gamepad.right_trigger != 0;
        if (mode == "delayed-cancel" && presenter.third_capture_returned.load()) {
          const auto now = std::chrono::steady_clock::now();
          if (!cancel_due) cancel_due = now + std::chrono::seconds(1);
          if (now >= *cancel_due) { stop.request_stop(); return; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    });
    if (mode == "cancel-after-input") {
      Need(driver.Arm(), "synthetic START publication");
      rex::input::X_INPUT_STATE state{};
      Need(driver.GetState(0, &state) == rex::X_RESULT{0} &&
          state.gamepad.buttons == mcla::autostart_input::kStartButton, "actual driver down read");
    }
    const bool returned = a::RunMotionConfig(stop.get_token(), presenter, driver, root);
    if (reader.joinable()) { reader.request_stop(); reader.join(); }
    if (captures) Need(start_edges == unsigned(success) && motion_edges == unsigned(success),
                       "one START pulse and one RT pulse only, none on pre-arm cancel");
    Need(returned == success && !reader_failed.load(), "exact expected workflow outcome");
    Need(stub::freezes == 1 && stub::disarms == 1 && !stub::installs && !stub::begins,
         "one terminal cut/disarm without runtime installation");
    if (!captures) Need(presenter.calls == 0 && !std::filesystem::exists(root / "motion-calibration"),
         "failed navigation never captures or starts motion");
    if (mode == "delayed-cancel") {
      Need(presenter.calls == 3 && !std::filesystem::exists(root / "motion-calibration") &&
           !driver.Poll().published_ms, "cancel during actual delay never arms or starts motion");
      const auto navigation = Json::parse(Read(root / "autostart" / "result.json"));
      Need(navigation.at("schema") == "mcla-autostart-navigation-v2" && navigation.at("delay_ms") == 8000 &&
           navigation.at("status") == "FAILED" && navigation.at("title_confirmed_ms").is_number_unsigned() &&
           navigation.at("start_due_ms").get<uint64_t>() == navigation.at("title_confirmed_ms").get<uint64_t>() + 8000 &&
           navigation.at("arm_before_ms").is_null() && navigation.at("arm_after_ms").is_null() &&
           navigation.at("published_ms").is_null(), "failed delayed receipt retains real pre-arm stage");
    }
    rex::input::X_INPUT_STATE neutral{};
    Need(driver.GetState(0, &neutral) == rex::X_RESULT{0} && !neutral.gamepad.buttons &&
         !neutral.gamepad.right_trigger && !driver.ArmMotion(), "failure permanently neutral");
    if (mode == "root-missing" || mode == "output-exists") {
      Need(!std::filesystem::exists(root / "motion-config" / "result.json"), "no overwrite or invented terminal");
    } else {
      const auto terminal = Json::parse(Read(root / "motion-config" / "result.json"));
      Need(terminal.at("schema") == "mcla-motion-config-terminal-v1" && terminal.at("token") == stub::token,
           "terminal schema/token");
      Need(terminal.at("status") == (success ? "COLLECTED_REVIEW_PENDING" : "FAILED") &&
           terminal.at("mode") == (captures ? "fixed60" : "stock") &&
           terminal.at("stop_requested") == !success && terminal.at("navigation_collected") == success &&
           terminal.at("motion_collected") == success && terminal.at("controller_disarmed") == true, "honest terminal outcome");
      Need(terminal.at("injection_requested") == true && terminal.at("input_injected") == (success || mode == "cancel-after-input"),
           "observed injection distinguished from request");
      for (const auto* key : {"os_input", "desktop_capture", "scene_proof", "movement_proof",
                             "physical_speed_proof", "fps_proof", "configuration_accepted"})
        Need(terminal.at(key).is_boolean() && !terminal.at(key).get<bool>(), "false proof flags");
      const auto configuration = Read(root / "motion-config" / "timer-config.json");
      Need(terminal.at("configuration_written") == !stub::fail_freeze &&
           terminal.at("configuration_bytes") == configuration.size(), "exact cut write status/size");
      Need(configuration == (stub::fail_freeze ? "" : stub::retained), "controller bytes not reserialized");
      Need(terminal.at("failure") == (success ? "" : stub::fail_freeze ? "CONFIGURATION_CUT_OR_WRITE_FAILED" : "NAVIGATION_FAILED"),
           "specific retained failure phase");
      if (success) {
        const auto navigation = Json::parse(Read(root / "autostart" / "result.json"));
        const auto motion = Json::parse(Read(root / "motion-calibration" / "result.json"));
        Need(navigation.at("schema") == (delayed ? "mcla-autostart-navigation-v2" : "mcla-autostart-navigation-v1"),
             "default v1 versus explicit delayed v2");
        std::set<std::string> expected_navigation_keys{
            "schema", "token", "status", "input_policy", "scene_proven", "fps_proven",
            "title_ready", "title_reason", "title_attempts", "input_status", "input_error",
            "published_ms", "released_ms", "down_ack_ms", "neutral_ack_ms", "down_reads", "frames"};
        if (delayed) expected_navigation_keys.insert({"delay_ms", "run_started_ms", "title_confirmed_ms",
                                                      "start_due_ms", "arm_before_ms", "arm_after_ms"});
        std::set<std::string> actual_navigation_keys;
        for (auto entry = navigation.begin(); entry != navigation.end(); ++entry)
          actual_navigation_keys.insert(entry.key());
        Need(actual_navigation_keys == expected_navigation_keys, "exact legacy or delayed field set");
        if (delayed) {
          const auto started = navigation.at("run_started_ms").get<uint64_t>();
          const auto title = navigation.at("title_confirmed_ms").get<uint64_t>();
          const auto due = navigation.at("start_due_ms").get<uint64_t>();
          const auto before = navigation.at("arm_before_ms").get<uint64_t>();
          const auto after = navigation.at("arm_after_ms").get<uint64_t>();
          const auto published = navigation.at("published_ms").get<uint64_t>();
          Need(navigation.at("delay_ms") == 8000 && started <= title && title + 8000 == due &&
               due <= before && before <= published && published <= after && after - started < 95000,
               "real delayed publication bracket and original budget");
          Need(navigation.at("released_ms").get<uint64_t>() >= published + 5000 &&
               navigation.at("neutral_ack_ms").get<uint64_t>() >= navigation.at("released_ms").get<uint64_t>(),
               "delayed START retains original hold and separate neutral");
        } else {
          for (const auto* key : {"delay_ms", "run_started_ms", "title_confirmed_ms", "start_due_ms", "arm_before_ms", "arm_after_ms"})
            Need(!navigation.contains(key), "default v1 contains no delay fields");
        }
        Need(navigation.at("frames").size() == 6 && motion.at("frames").size() == 6 &&
             presenter.calls == navigation.at("title_attempts").get<unsigned>() + 11,
             "same navigation and motion capture schedule");
        Need(navigation.at("status") == "INPUT_COMPLETE_SCENE_REVIEW_PENDING" &&
             motion.at("status") == "MOTION_CALIBRATION_COLLECTED_REVIEW_PENDING" &&
             motion.at("right_trigger") == 255 && motion.at("down_read_count").get<uint64_t>() > 0,
             "actual driver acknowledged START and full-amplitude motion");
        for (const auto* key : {"scene_proven", "movement_proven", "physical_speed_proven", "fps_proven"})
          Need(motion.at(key) == false, "synthetic success is not physical proof");
        for (const auto* directory : {"autostart", "motion-calibration"}) {
          unsigned entries = 0;
          for (const auto& file : std::filesystem::directory_iterator(root / directory)) {
            Need(file.is_regular_file(), "only regular output files"); ++entries;
          }
          Need(entries == 7, "exact receipt plus six images");
        }
      }
    }
    std::cout << "motion config actual wrapper: " << checks << " checks PASS mode=" << mode << '\n';
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
