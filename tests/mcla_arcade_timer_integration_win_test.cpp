// Actual RunMotionConfig -> Run -> RunArcade -> RunMotion, synthetic only.
// MODE(stock|raw|off) SCENARIO FRESH_ABSOLUTE_ROOT PINNED_TITLE_BMP.
// SCENARIO: success,before-fail,cancel-after,finish-fail,write-fail,config-fail,
//           lower-equal,lower-outside,upper-equal,wrong-frequency. OFF: success only.
// No Runtime/GPU/window/game/OS input. Controller and timer-observer lifecycle
// are STUBS; captured pixels and QPC are synthetic. Actual files are retained.
// The test-only clock is advanced solely by main-thread Pause; the background
// motion poller may read it but NEVER advances time or reads the controller.
// A local WriteFile boundary fails only the owned timer-calls.json handle.
// Neither that substitution nor fake clocks are part of a production build.
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <stop_token>
#include <string>
#include <thread>

namespace timer_integration {
std::atomic<uint64_t> milliseconds{1000};
std::thread::id main_thread;
std::function<void()> sample_input;
std::stop_source* cancellation{};
std::string scenario;
unsigned writes_failed{}, lifecycle_calls{}, begins{}, finishes{}, disarms{}, windows{};
unsigned config_freezes_at_disarm[3]{};
uint64_t last_capture_qpc{}, before_capture_qpc{};
bool installed_called{}, invoked{}, begun{}, finished{};
uint64_t Tick() { return milliseconds.load(std::memory_order_acquire) * 10000 + 1; }
BOOL WINAPI Counter(LARGE_INTEGER* out) { out->QuadPart = static_cast<LONGLONG>(Tick()); return TRUE; }
BOOL WINAPI Frequency(LARGE_INTEGER* out) { out->QuadPart = 10000000; return TRUE; }
BOOL WINAPI Write(HANDLE file, LPCVOID data, DWORD bytes, LPDWORD written, LPOVERLAPPED overlap) {
  // The stub receipt is <256 bytes; BMP headers/rows need no path lookup.
  if (scenario == "write-fail" && bytes < 256 && bytes != 54) {
    wchar_t path[32768]{};
    const DWORD n = GetFinalPathNameByHandleW(file, path, 32768, FILE_NAME_NORMALIZED);
    if (n && n < 32768 && std::wstring(path, n).ends_with(L"\\motion-config\\timer-calls.json")) {
      ++writes_failed; if (written) *written = 0;
      SetLastError(ERROR_WRITE_FAULT); return FALSE;
    }
  }
  return ::WriteFile(file, data, bytes, written, overlap);
}
}
#define MCLA_AUTOSTART_TEST_CLOCK 1
#define QueryPerformanceCounter timer_integration::Counter
#define QueryPerformanceFrequency timer_integration::Frequency
#define WriteFile timer_integration::Write
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#undef WriteFile
#undef QueryPerformanceFrequency
#undef QueryPerformanceCounter
#include <vector>

#if !defined(MCLA_ARCADE_TIMER_PROBE) || !defined(MCLA_INPUT_RETURN_PROBE) || \
    !defined(MCLA_RAW_CONFIG_EXPERIMENT) || !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || \
    !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || !defined(MCLA_UPDATE_TIMING_PROBE)
#error Compile explicit timer/config/motion/raw/fixed60/census/scout/update branches.
#endif
REXCVAR_DEFINE_BOOL(mcla_arcade_timer_probe, false, "Test", "Synthetic observer fixture only")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
namespace mcla::autostart_test_clock {
uint64_t NowMs() { return timer_integration::milliseconds.load(std::memory_order_acquire); }
bool Pause(std::stop_token stop, uint64_t ms) {
  if (std::this_thread::get_id() != timer_integration::main_thread) {
    std::this_thread::yield(); return !stop.stop_requested();
  }
  if (stop.stop_requested()) return false;
  if (ms > 150000 || NowMs() > 200000) throw std::runtime_error("synthetic clock budget");
  if (timer_integration::sample_input) timer_integration::sample_input();
  while (ms && !stop.stop_requested()) {
    const auto step = std::min<uint64_t>(ms, 10);
    timer_integration::milliseconds.fetch_add(step, std::memory_order_release); ms -= step;
    if (timer_integration::sample_input) timer_integration::sample_input();
  }
  return !stop.stop_requested();
}
}
namespace mcla::input_return {
bool Enabled() { return false; }
void Enable() { ++timer_integration::lifecycle_calls; throw std::runtime_error("no census"); }
void Install(rex::Runtime&) { ++timer_integration::lifecycle_calls; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++timer_integration::lifecycle_calls; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++timer_integration::lifecycle_calls; throw std::runtime_error("no census freeze"); }
void Disarm() noexcept { ++timer_integration::lifecycle_calls; }
}
namespace timer_integration { mcla::timer_journal::Window window; }
namespace mcla::arcade_timer {
bool Enabled() { return REXCVAR_GET(mcla_arcade_timer_probe); }
void Install(rex::Runtime&) { timer_integration::installed_called = true; throw std::runtime_error("no runtime install"); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace timer_integration;
  ++begins;
  if (begun || token != stub::token || end != begin + 12 * frequency ||
      frequency != 10000000 || begin != Tick() || !before_capture_qpc || begin < before_capture_qpc)
    throw std::runtime_error("bad synthetic Begin");
  window = {begin, end, frequency}; begun = true;
  if (scenario == "cancel-after") cancellation->request_stop();
}
timer_journal::Window ObservationWindow(const std::string& token) {
  ++timer_integration::windows;
  if (!timer_integration::begun || token != stub::token) throw std::runtime_error("no synthetic window");
  return timer_integration::window;
}
std::string Finish(std::stop_token stop, const std::string& token) {
  using namespace timer_integration;
  ++finishes;
  if (!begun || finished || stop.stop_requested() || token != stub::token || Tick() < window.end_qpc || stub::freezes)
    throw std::runtime_error("bad synthetic Finish");
  finished = true;
  if (scenario == "finish-fail") throw std::runtime_error("injected timer Finish failure");
  return std::string("{\"synthetic_arcade_timer_fixture\":true,\"token\":\"") + token + "\"}";
}
void Disarm() noexcept {
  const auto index = timer_integration::disarms++;
  if (index < 3) timer_integration::config_freezes_at_disarm[index] = stub::freezes;
}
void Invoke(PPCFunc*, PPCContext&, uint8_t*) {
  timer_integration::invoked = true; throw std::runtime_error("no original or timer callback");
}
}
namespace {
using namespace std::chrono_literals;
void ExactKeys(const Json& object, const std::set<std::string>& expected) {
  Need(object.is_object(), "JSON object"); std::set<std::string> actual;
  for (const auto& [key, value] : object.items()) actual.insert(key);
  Need(actual == expected, "exact JSON fields");
}
class TimerPresenter final : public rex::ui::Presenter {
 public:
  TimerPresenter() : Presenter({}) {}
  rex::ui::RawImage title;
  unsigned calls{};
  std::vector<uint64_t> ticks{0}, frequencies{0};
  rex::ui::Surface::TypeFlags GetSupportedSurfaceTypes() const override { return 0; }
  bool CaptureGuestOutput(rex::ui::RawImage&) override { throw std::runtime_error("wrong capture API"); }
  rex::ui::GuestCaptureStatus CaptureGuestOutputWithProvenance(rex::ui::GuestOutputCapture& out) override {
    ++calls;
    Need(!title.data.empty() && calls <= 43, "bounded synthetic capture");
    if (calls == 19 && timer_integration::scenario == "before-fail")
      throw std::runtime_error("injected before-image failure");
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
    uint64_t tick = timer_integration::Tick(), frequency = 10000000;
    if (calls == 20) {
      if (timer_integration::scenario == "lower-equal") tick = timer_integration::window.begin_qpc;
      if (timer_integration::scenario == "lower-outside") tick = timer_integration::window.begin_qpc - 1;
      if (timer_integration::scenario == "upper-equal") tick = timer_integration::window.end_qpc;
      if (timer_integration::scenario == "wrong-frequency") ++frequency;
    }
    auto& p = out.provenance;
    p.presenter_epoch = 1; p.acquired_sequence = calls; p.qpc_frequency = frequency;
    p.refresh_cpu_begin_qpc = p.refresh_cpu_end_qpc = p.acquire_before_qpc = p.acquire_after_qpc =
        p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = tick;
    ticks.push_back(tick); frequencies.push_back(frequency);
    timer_integration::last_capture_qpc = tick;
    if (calls == 19) timer_integration::before_capture_qpc = tick;
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
                 const TimerPresenter& presenter) {
  ExactKeys(p, {"width","height","stride","presenter_epoch","acquired_sequence","qpc_frequency",
      "refresh_cpu_begin_qpc","refresh_cpu_end_qpc","acquire_before_qpc","acquire_after_qpc",
      "copy_submit_before_qpc","copy_completion_observed_qpc","pixels_ready_qpc"});
  Need(p.at("width") == 1280 && p.at("height") == 720 && p.at("stride") == 5120 &&
      p.at("presenter_epoch") == 1 && p.at("acquired_sequence") == sequence &&
      p.at("qpc_frequency") == presenter.frequencies.at(sequence), "exact synthetic identity/layout/frequency");
  for (const auto* key : {"refresh_cpu_begin_qpc","refresh_cpu_end_qpc","acquire_before_qpc",
      "acquire_after_qpc","copy_submit_before_qpc","copy_completion_observed_qpc","pixels_ready_qpc"})
    Need(p.at(key) == presenter.ticks.at(sequence), "all exact synthetic provenance timestamps");
  const auto bytes = Read(file, 4 * 1024 * 1024);
  auto le = [&](size_t offset, unsigned count) {
    uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i) value |= uint32_t(uint8_t(bytes.at(offset+i))) << (8*i);
    return value;
  };
  constexpr size_t payload = size_t(1280) * 720 * 3;
  Need(bytes.size() == 54 + payload && le(0,2) == 0x4d42 && le(2,4) == bytes.size() &&
      le(10,4) == 54 && le(14,4) == 40 && le(18,4) == 1280 && le(22,4) == 720 &&
      le(26,2) == 1 && le(28,2) == 24 && le(30,4) == 0 && le(34,4) == payload,
      "exact complete production BMP transport");
  for (uint32_t y = 0; y < 720; ++y) for (uint32_t x = 0; x < 1280; ++x) {
    const size_t at = 54 + (size_t(719-y) * 1280 + x) * 3, rgba = size_t(y) * 5120 + x * 4;
    const uint8_t red = sequence == 3 ? presenter.title.data.at(rgba) : uint8_t(sequence);
    const uint8_t green = sequence == 3 ? presenter.title.data.at(rgba+1) : 37;
    const uint8_t blue = sequence == 3 ? presenter.title.data.at(rgba+2) : 83;
    if (uint8_t(bytes[at]) != blue || uint8_t(bytes[at+1]) != green || uint8_t(bytes[at+2]) != red)
      throw std::runtime_error("BMP payload differs from exact synthetic source");
  }
  Need(true, "every retained pixel compared, not exit-code-only acceptance");
}
}
int main(int argc, char** argv) {
  std::jthread watchdog([](std::stop_token stop) {
    const auto deadline = std::chrono::steady_clock::now() + 60s;
    while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(10ms);
    if (!stop.stop_requested()) std::_Exit(4);
  });
  int exit_code = 1;
  try {
    Need(argc == 5, "MODE SCENARIO FRESH_ABSOLUTE_ROOT PINNED_TITLE_BMP");
    const std::string mode = argv[1], scenario = argv[2];
    const std::set<std::string> scenarios{"success","before-fail","cancel-after","finish-fail",
        "write-fail","config-fail","lower-equal","lower-outside","upper-equal","wrong-frequency"};
    Need((mode == "stock" || mode == "raw" || mode == "off") && scenarios.contains(scenario) &&
        (mode != "off" || scenario == "success"), "exact mode/scenario");
    const bool enabled = mode != "off", before_fail = scenario == "before-fail";
    const bool cancel = scenario == "cancel-after", config_ok = scenario != "config-fail";
    const bool bad_bound = scenario == "lower-outside" || scenario == "upper-equal" || scenario == "wrong-frequency";
    const bool full = !before_fail && !cancel && !bad_bound;
    const bool timer_ok = enabled && !before_fail && !cancel && scenario != "finish-fail" && scenario != "write-fail";
    const bool success = full && config_ok && (!enabled || timer_ok);
    const std::filesystem::path root = argv[3];
    Need(root.is_absolute() && root == root.lexically_normal() && !root.filename().empty() &&
        !std::filesystem::exists(root), "fresh absolute owned output only");
    auto parent = mcla::launcher::fs::OpenDirectory(root.parent_path().wstring());
    Need(parent.has_value(), "hold safe existing parent");
    NoCapture loader; LoadTitle(loader, std::filesystem::path(argv[4]));
    Need(std::filesystem::create_directory(root), "create only supplied fresh root");
    timer_integration::main_thread = std::this_thread::get_id(); timer_integration::scenario = scenario;
    REXCVAR_SET(mcla_arcade_timer_probe, enabled); REXCVAR_SET(mcla_arcade_config_probe, true);
    REXCVAR_SET(mcla_autostart_probe, true); REXCVAR_SET(mcla_autostart_token, stub::token);
    REXCVAR_SET(mcla_arcade_navigation_probe, true); REXCVAR_SET(mcla_motion_calibration_probe, true);
    REXCVAR_SET(mcla_motion_config_probe, true); REXCVAR_SET(mcla_timer_config_probe, true);
    REXCVAR_SET(mcla_raw_config_experiment, mode == "raw"); REXCVAR_SET(mcla_fixed60_config_experiment, false);
    REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
    Need(Admits(), "actual admission"); stub::fail_freeze = !config_ok;
    a::Driver driver; Need(driver.Setup() == rex::X_STATUS{0}, "actual driver setup");
    TimerPresenter presenter; presenter.title = std::move(loader.synthetic_title);
    std::stop_source stop; timer_integration::cancellation = &stop;
    std::vector<uint16_t> buttons; unsigned motion_edges = 0;
    uint16_t prior_button = 0; bool prior_motion = false;
    timer_integration::sample_input = [&] {
      rex::input::X_INPUT_STATE state{};
      Need(driver.GetState(0, &state) == rex::X_RESULT{0}, "synthetic controller read");
      const auto& pad = state.gamepad;
      Need((!pad.buttons || pad.buttons == 0x10 || pad.buttons == 0x200 || pad.buttons == 0x1000) &&
          (!pad.right_trigger || pad.right_trigger == 255) && !(pad.buttons && pad.right_trigger) &&
          !pad.left_trigger && !pad.thumb_lx && !pad.thumb_ly && !pad.thumb_rx && !pad.thumb_ry,
          "only exact bounded internal buttons and RT255");
      if (pad.buttons && !prior_button) buttons.push_back(pad.buttons);
      if (pad.right_trigger && !prior_motion) ++motion_edges;
      prior_button = pad.buttons; prior_motion = pad.right_trigger != 0;
    };
    const auto real_begin = std::chrono::steady_clock::now();
    const bool returned = a::RunMotionConfig(stop.get_token(), presenter, driver, root);
    const auto real_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - real_begin).count();
    timer_integration::sample_input = {};
    Need(returned == success, "exact workflow outcome");
    Need(stub::freezes == 1 && stub::disarms == 1 && !stub::installs && !stub::begins &&
        !timer_integration::lifecycle_calls && !timer_integration::installed_called && !timer_integration::invoked,
        "configuration always cut/disarmed once; no Runtime, census or real timer");
    Need(timer_integration::begins == unsigned(enabled && !before_fail) &&
        timer_integration::finishes == unsigned(enabled && !before_fail && !cancel), "exact Begin/Finish counts");
    const unsigned expected_disarms = !enabled ? 0u : timer_ok ? 1u : 2u;
    Need(timer_integration::disarms == expected_disarms &&
        timer_integration::writes_failed == unsigned(scenario == "write-fail"), "failure and final disarms; targeted write failure");
    if (enabled) {
      Need(timer_integration::config_freezes_at_disarm[expected_disarms-1] == 1,
          "final timer disarm follows configuration Freeze attempt");
      if (expected_disarms == 2)
        Need(timer_integration::config_freezes_at_disarm[0] == 0,
            "timer failure disarms before independent configuration Freeze");
    }
    Need(timer_integration::windows == (!enabled ? 0u : full ? 25u : bad_bound ? 2u : 1u),
        "every timed image and final cut use exact timer window");
    Need(presenter.calls == (full ? 43u : bad_bound ? 20u : 19u), "exact presenter call count");
    Need(buttons == std::vector<uint16_t>{0x10,0x10,0x200,0x1000,0x1000,0x1000,0x1000} &&
        motion_edges == unsigned(!before_fail && !cancel), "all seven button edges and exact motion edge");
    Need(timer_integration::milliseconds >= (full ? 100000u : 90000u) && timer_integration::milliseconds < 150000,
        "same bounded schedule with test-only clock advancement");
    rex::input::X_INPUT_STATE neutral{};
    Need(driver.GetState(0, &neutral) == rex::X_RESULT{0} && !neutral.gamepad.buttons &&
        !neutral.gamepad.right_trigger && !driver.ArmMotion(), "terminal neutral and no rearm");
    const auto terminal = Json::parse(Read(root / "motion-config/result.json"));
    const std::string configuration = Read(root / "motion-config/timer-config.json");
    Need(configuration == (config_ok ? stub::retained : ""), "exact configuration bytes even after timer failure");
    std::string failure = success ? "" : !config_ok ? "CONFIGURATION_CUT_OR_WRITE_FAILED" :
        !full ? "MOTION_FAILED" : scenario == "finish-fail" ? "ARCADE_TIMER_FINISH_FAILED" : "ARCADE_TIMER_WRITE_FAILED";
    Json expected_terminal{{"schema", enabled ? "mcla-arcade-timer-terminal-v1" : "mcla-arcade-config-terminal-v1"},
        {"token",stub::token},{"mode",mode == "raw" ? "raw" : "stock"},
        {"status",success ? "COLLECTED_REVIEW_PENDING" : "FAILED"},{"failure",failure},
        {"stop_requested",cancel},{"navigation_collected",true},{"motion_collected",full},
        {"configuration_written",config_ok},{"configuration_file","timer-config.json"},
        {"configuration_bytes",configuration.size()},{"controller_disarmed",true},
        {"injection_requested",true},{"input_injected",true},{"os_input",false},{"desktop_capture",false},
        {"scene_proof",false},{"movement_proof",false},{"physical_speed_proof",false},{"fps_proof",false},
        {"configuration_accepted",false}};
    if (enabled) {
      const std::string timer_text = Read(root / "motion-config/timer-calls.json");
      const std::string wanted = timer_ok ? std::string("{\"synthetic_arcade_timer_fixture\":true,\"token\":\"") + stub::token + "\"}" : "";
      Need(timer_text == wanted, "exact synthetic timer receipt or retained empty failed output");
      const std::string timer_failure = before_fail ? "ARCADE_TIMER_WINDOW_UNAVAILABLE" : cancel ? "ARCADE_TIMER_WAIT_FAILED" :
          scenario == "finish-fail" ? "ARCADE_TIMER_FINISH_FAILED" : scenario == "write-fail" ? "ARCADE_TIMER_WRITE_FAILED" : "";
      expected_terminal.update(Json{{"timer_written",timer_ok},{"timer_file","timer-calls.json"},
          {"timer_bytes",timer_text.size()},{"timer_failure",timer_failure},{"timer_disarmed",true},
          {"before_frame_timer_covered",false}});
    }
    Need(terminal == expected_terminal, "entire exact terminal; OFF retains legacy schema and fields");
    std::set<std::filesystem::path> expected_files{"motion-config/result.json","motion-config/timer-config.json",
        "autostart/result.json","arcade-navigation/result.json","motion-calibration/result.json"};
    if (enabled) expected_files.insert("motion-config/timer-calls.json");
    const auto nav = Json::parse(Read(root / "autostart/result.json"));
    const auto arcade = Json::parse(Read(root / "arcade-navigation/result.json"));
    const auto motion = Json::parse(Read(root / "motion-calibration/result.json"));
    Need(nav.at("frames").size() == 6 && nav.at("title_attempts") == 3 && nav.at("title_ready") == true &&
        nav.at("status") == "INPUT_COMPLETE_SCENE_REVIEW_PENDING", "actual START navigation completed");
    Need(arcade.at("frames").size() == 10 && arcade.at("commands").size() == 6 && arcade.at("failure") == "" &&
        arcade.at("status") == "ARCADE_NAVIGATION_COLLECTED_REVIEW_PENDING", "actual six-command Arcade completed");
    for (unsigned i = 0; i < 6; ++i) {
      const auto& command = arcade.at("commands")[i];
      Need(command.at("sequence") == i+1 && command.at("status") == int(mcla::arcade_input::Status::Succeeded) &&
          command.at("down_ack_ms").get<uint64_t>() >= command.at("published_ms").get<uint64_t>() &&
          command.at("neutral_ack_ms").get<uint64_t>() >= command.at("released_ms").get<uint64_t>(),
          "six exact menu command ACKs");
    }
    Need(motion.at("frames").size() == (full ? 25u : bad_bound ? 2u : 1u) &&
        motion.at("status") == (full ? "MOTION_CALIBRATION_COLLECTED_REVIEW_PENDING" : "FAILED") &&
        motion.at("right_trigger") == 255 && motion.at("capture_retry_count") == 0,
        "exact motion success or preserved failure prefix");
    for (const auto* receipt : {&nav,&arcade,&motion}) {
      Need(receipt->at("token") == stub::token && receipt->at("fps_proven") == false, "shared token; never FPS proof");
    }
    unsigned saved = 0;
    auto image = [&](const std::filesystem::path& name, const Json& p, unsigned sequence) {
      expected_files.insert(name); VerifyImage(root / name, p, sequence, presenter); ++saved;
    };
    for (unsigned i = 0; i < 6; ++i)
      image(std::filesystem::path("autostart") / (i ? "post-" + std::to_string(i) + ".bmp" : "title.bmp"),
          nav.at("frames")[i], i+3);
    for (unsigned i = 0; i < 10; ++i)
      image(std::filesystem::path("arcade-navigation") / ("frame-" + std::to_string(i) + ".bmp"),
          arcade.at("frames")[i].at("provenance"), i+9);
    for (unsigned i = 0; i < motion.at("frames").size(); ++i) {
      const auto& row = motion.at("frames")[i];
      Need(row.at("slot") == i, "all motion rows retained in order");
      if (!i) Need(row.at("due_ms").is_null(), "before has no synthetic due time");
      else Need(motion.at("capture_offsets_ms")[i-1] == i*250 &&
          row.at("due_ms") == motion.at("published_ms").get<uint64_t>() + i*250,
          "fixed motion offsets preserved");
      if (row.at("saved") == true)
        image(std::filesystem::path("motion-calibration") / (i ? "motion-" + std::to_string(i) + ".bmp" : "before.bmp"),
            row.at("provenance"), i+19);
      else Need((before_fail && i == 0) || (bad_bound && i == 1), "only declared rejected frame lacks pixels");
    }
    Need(saved == (full ? 41u : before_fail ? 16u : 17u), "exact retained BMP count");
    std::set<std::filesystem::path> actual;
    for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
      Need(!item.is_symlink(), "no output symlink");
      if (item.is_directory()) continue;
      Need(item.is_regular_file(), "regular artifact only"); actual.insert(item.path().lexically_relative(root));
    }
    Need(actual == expected_files, "exact full output inventory without extra files");
    std::cout << "ARCADE_TIMER_INTEGRATION mode=" << mode << " scenario=" << scenario
        << " checks=" << checks << " failures=0 saved_bmps=" << saved << " artifacts=" << actual.size()
        << " synthetic_ms=" << timer_integration::milliseconds.load() << " real_ms=" << real_ms
        << " TEST_CLOCK_TIMER_STUB_AND_SYNTHETIC_PIXELS_ONLY\n";
    std::cout.flush(); std::cerr.flush(); exit_code = std::cout.good() && std::cerr.good() ? 0 : 2;
  } catch (const std::exception& error) {
    timer_integration::sample_input = {};
    std::cerr << "FAIL " << error.what() << '\n'; std::cerr.flush(); exit_code = std::cerr.good() ? 1 : 2;
  }
  watchdog.request_stop(); watchdog.join(); return exit_code;
}
