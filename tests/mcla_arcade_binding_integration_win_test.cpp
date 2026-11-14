// Real RunMotionConfig -> Run -> RunArcade -> RunMotion, synthetic presenters
// and named phase/timer/config STUBS. One process per SCENARIO FRESH_ABSOLUTE_ROOT
// PINNED_TITLE_BMP. No Runtime, game, GPU, window, OS input, or production hooks.
// Include this TU only; link launcher_fs_win.cpp + the retained integration SDK
// dependencies. Required macros: AUTOSTART, MOTION_CALIBRATION, MOTION_CONFIG,
// TIMER_CONFIG, RAW_CONFIG_EXPERIMENT, FIXED60_CONFIG_EXPERIMENT,
// GUEST_SOURCE_SCOUT, UPDATE_TIMING, INPUT_RETURN, ARCADE_TIMER,
// ARCADE_UPDATE_BINDING, PHASE_CENSUS (each with its usual _PROBE suffix).
// Test-only QPC/Pause/WriteFile seams never enter a production build.
#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
namespace stub { extern unsigned freezes; }
namespace binding_integration {
std::atomic<uint64_t> milliseconds{1000};
std::thread::id main_thread;
std::function<void()> sample_input;
std::stop_source* cancellation{};
std::string scenario;
struct Event { const char* name; unsigned config_freezes; };
std::array<Event, 64> events{};
unsigned event_count{}, forbidden_calls{}, timer_begins{}, timer_finishes{}, timer_disarms{}, windows{};
unsigned phase_begins{}, phase_finishes{}, phase_disarms{}, write_failures{};
bool contract_failed{}, event_overflow{}, phase_begun{}, timer_begun{}, timer_stopped{}, phase_stopped{};
uint64_t before_qpc{}, phase_begin{}, phase_end{}, phase_frequency{};
void EventLog(const char* name) noexcept {
  if (event_count == events.size()) { event_overflow = true; return; }
  events[event_count++] = {name, stub::freezes};
}
void Contract(bool ok) {
  if (!ok) { contract_failed = true; throw std::runtime_error("stub lifecycle contract violation"); }
}
uint64_t Tick() { return milliseconds.load(std::memory_order_acquire) * 10000 + 1; }
BOOL WINAPI Counter(LARGE_INTEGER* out) { out->QuadPart = static_cast<LONGLONG>(Tick()); return TRUE; }
BOOL WINAPI Frequency(LARGE_INTEGER* out) { out->QuadPart = 10000000; return TRUE; }
BOOL WINAPI Write(HANDLE file, LPCVOID data, DWORD bytes, LPDWORD written, LPOVERLAPPED overlap) {
  // BMP header and fixed 1280x720 RGB row sizes do not need a path lookup.
  if (bytes != 54 && bytes != 3840) {
    wchar_t path[32768]{};
    const DWORD n = GetFinalPathNameByHandleW(file, path, 32768, FILE_NAME_NORMALIZED);
    if (n && n < 32768) {
      const std::wstring name(path, n);
      const bool timer = name.ends_with(L"\\motion-config\\timer-calls.json");
      const bool phase = name.ends_with(L"\\motion-config\\phase-census.json");
      const bool config = name.ends_with(L"\\motion-config\\timer-config.json");
      const bool terminal = name.ends_with(L"\\motion-config\\result.json");
      if (timer) EventLog("timer-write");
      if (phase) EventLog("phase-write");
      if (config) EventLog("config-write");
      if (terminal) EventLog("terminal-write");
      if ((timer && scenario == "timer-write-fail") || (phase && scenario == "phase-write-fail") ||
          (terminal && scenario == "terminal-write-fail")) {
        ++write_failures; if (written) *written = 0;
        SetLastError(ERROR_WRITE_FAULT); return FALSE;
      }
    }
  }
  return ::WriteFile(file, data, bytes, written, overlap);
}
}
#define MCLA_AUTOSTART_TEST_CLOCK 1
#define QueryPerformanceCounter binding_integration::Counter
#define QueryPerformanceFrequency binding_integration::Frequency
#define WriteFile binding_integration::Write
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#undef WriteFile
#undef QueryPerformanceFrequency
#undef QueryPerformanceCounter
#include <vector>

#if !defined(MCLA_ARCADE_UPDATE_BINDING_PROBE) || !defined(MCLA_PHASE_CENSUS_PROBE) || \
    !defined(MCLA_ARCADE_TIMER_PROBE) || !defined(MCLA_INPUT_RETURN_PROBE) || \
    !defined(MCLA_RAW_CONFIG_EXPERIMENT) || !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || \
    !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || !defined(MCLA_UPDATE_TIMING_PROBE)
#error Compile all explicit standalone binding and legacy admission branches.
#endif
#define FIXTURE_FLAG(name) REXCVAR_DEFINE_BOOL(name, false, "Test", "Synthetic lifecycle fixture only") \
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
FIXTURE_FLAG(mcla_arcade_timer_probe);
FIXTURE_FLAG(mcla_arcade_update_binding_probe);
FIXTURE_FLAG(mcla_phase_census_probe);
#undef FIXTURE_FLAG
namespace mcla::autostart_test_clock {
uint64_t NowMs() { return binding_integration::milliseconds.load(std::memory_order_acquire); }
bool Pause(std::stop_token stop, uint64_t ms) {
  if (std::this_thread::get_id() != binding_integration::main_thread) {
    std::this_thread::yield(); return !stop.stop_requested();
  }
  if (stop.stop_requested()) return false;
  if (ms > 150000 || NowMs() > 200000) throw std::runtime_error("synthetic clock budget");
  if (binding_integration::sample_input) binding_integration::sample_input();
  while (ms && !stop.stop_requested()) {
    const auto step = std::min<uint64_t>(ms, 10);
    binding_integration::milliseconds.fetch_add(step, std::memory_order_release); ms -= step;
    if (binding_integration::sample_input) binding_integration::sample_input();
  }
  return !stop.stop_requested();
}
}
namespace mcla::input_return {
bool Enabled() { return false; }
void Enable() { ++binding_integration::forbidden_calls; throw std::runtime_error("no census"); }
void Install(rex::Runtime&) { ++binding_integration::forbidden_calls; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++binding_integration::forbidden_calls; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++binding_integration::forbidden_calls; throw std::runtime_error("no census freeze"); }
void Disarm() noexcept { ++binding_integration::forbidden_calls; }
}
namespace binding_integration { mcla::timer_journal::Window window; }
namespace mcla::arcade_timer {
bool Enabled() { return REXCVAR_GET(mcla_arcade_timer_probe); }
bool UpdateBindingEnabled() { return REXCVAR_GET(mcla_arcade_update_binding_probe); }
void Install(rex::Runtime&) { ++binding_integration::forbidden_calls; throw std::runtime_error("no Runtime install"); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace binding_integration;
  ++timer_begins; EventLog("timer-begin");
  Contract(!timer_begun && phase_begun && !phase_stopped && token == stub::token &&
      begin == phase_begin && end == phase_end && frequency == phase_frequency && begin == Tick());
  if (scenario == "timer-begin-fail") throw std::runtime_error("injected timer Begin failure");
  window = {begin, end, frequency}; timer_begun = true;
  if (scenario == "cancel-after") cancellation->request_stop();
}
timer_journal::Window ObservationWindow(const std::string& token) {
  ++binding_integration::windows;
  if (!binding_integration::timer_begun || token != stub::token) throw std::runtime_error("no synthetic window");
  return binding_integration::window;
}
std::string Finish(std::stop_token stop, const std::string& token) {
  using namespace binding_integration;
  ++timer_finishes; EventLog("timer-finish");
  Contract(timer_begun && !timer_stopped && phase_begun && !phase_stopped && !stop.stop_requested() &&
      token == stub::token && Tick() >= window.end_qpc && !stub::freezes);
  timer_stopped = true;
  if (scenario == "timer-finish-fail" || scenario == "compound-fail")
    throw std::runtime_error("injected timer Finish failure");
  return std::string("{\"synthetic_timer_stub\":true,\"token\":\"") + token + "\"}";
}
void Disarm() noexcept {
  ++binding_integration::timer_disarms; binding_integration::timer_stopped = true;
  binding_integration::EventLog("timer-disarm");
}
void Invoke(PPCFunc*, PPCContext&, uint8_t*) {
  ++binding_integration::forbidden_calls; throw std::runtime_error("no timer original");
}
}
namespace mcla::phase_census {
bool Enabled() { return REXCVAR_GET(mcla_phase_census_probe); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace binding_integration;
  ++phase_begins; EventLog("phase-begin");
  Contract(!phase_begun && !timer_begins && !phase_stopped && token == stub::token &&
      frequency == 10000000 && begin == Tick() && end == begin + 12 * frequency &&
      before_qpc && begin >= before_qpc);
  if (scenario == "phase-begin-fail") throw std::runtime_error("injected phase Begin failure");
  phase_begin = begin; phase_end = end; phase_frequency = frequency; phase_begun = true;
}
std::string Finish(std::stop_token stop, const std::string& token) {
  using namespace binding_integration;
  ++phase_finishes; EventLog("phase-finish");
  // Timer Finish completed/failed, or cancellation/unavailable-window Disarm
  // happened first. Phase was not stopped while timer drain was in progress.
  Contract(timer_stopped && !phase_stopped && !stub::freezes && token == stub::token);
  phase_stopped = true;
  if (!phase_begun || stop.stop_requested() || scenario == "phase-finish-fail" || scenario == "compound-fail")
    throw std::runtime_error("injected/unavailable phase Finish");
  return std::string("{\"synthetic_phase_stub\":true,\"token\":\"") + token + "\"}";
}
void Disarm() noexcept {
  ++binding_integration::phase_disarms; binding_integration::phase_stopped = true;
  binding_integration::EventLog("phase-disarm");
}
}
namespace {
using namespace std::chrono_literals;
class BindingPresenter final : public rex::ui::Presenter {
 public:
  BindingPresenter() : Presenter({}) {}
  rex::ui::RawImage title;
  unsigned calls{};
  std::vector<uint64_t> ticks{0};
  rex::ui::Surface::TypeFlags GetSupportedSurfaceTypes() const override { return 0; }
  bool CaptureGuestOutput(rex::ui::RawImage&) override { throw std::runtime_error("wrong capture API"); }
  rex::ui::GuestCaptureStatus CaptureGuestOutputWithProvenance(rex::ui::GuestOutputCapture& out) override {
    ++calls;
    Need(!title.data.empty() && calls <= 43, "bounded synthetic capture");
    const auto& scenario = binding_integration::scenario;
    if ((calls == 1 && scenario == "nav-fail") || (calls == 19 && scenario == "before-fail") ||
        (calls == 20 && scenario == "motion-fail")) throw std::runtime_error("injected capture failure");
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
    const auto tick = binding_integration::Tick(); auto& p = out.provenance;
    p.presenter_epoch = 1; p.acquired_sequence = calls; p.qpc_frequency = 10000000;
    p.refresh_cpu_begin_qpc = p.refresh_cpu_end_qpc = p.acquire_before_qpc = p.acquire_after_qpc =
        p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = tick;
    ticks.push_back(tick); if (calls == 19) binding_integration::before_qpc = tick;
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
void VerifyImage(const std::filesystem::path& path, const Json& p, unsigned sequence,
                 const BindingPresenter& presenter) {
  const auto tick = presenter.ticks.at(sequence);
  const Json expected_p{{"width",1280},{"height",720},{"stride",5120},{"presenter_epoch",1},
      {"acquired_sequence",sequence},{"qpc_frequency",10000000},{"refresh_cpu_begin_qpc",tick},
      {"refresh_cpu_end_qpc",tick},{"acquire_before_qpc",tick},{"acquire_after_qpc",tick},
      {"copy_submit_before_qpc",tick},{"copy_completion_observed_qpc",tick},{"pixels_ready_qpc",tick}};
  Need(p == expected_p, "entire exact synthetic image provenance");
  const auto bytes = Read(path, 4 * 1024 * 1024);
  auto le = [&](size_t at, unsigned n) {
    uint32_t value = 0; for (unsigned i = 0; i < n; ++i) value |= uint32_t(uint8_t(bytes.at(at+i))) << (8*i);
    return value;
  };
  constexpr size_t payload = size_t(1280) * 720 * 3;
  Need(bytes.size() == 54 + payload && le(0,2) == 0x4d42 && le(2,4) == bytes.size() &&
       le(10,4) == 54 && le(14,4) == 40 && le(18,4) == 1280 && le(22,4) == 720 &&
       le(26,2) == 1 && le(28,2) == 24 && le(30,4) == 0 && le(34,4) == payload, "complete BMP header/layout");
  for (uint32_t y = 0; y < 720; ++y) for (uint32_t x = 0; x < 1280; ++x) {
    const auto at = 54 + (size_t(719-y)*1280+x)*3, rgba = size_t(y)*5120+x*4;
    const uint8_t red = sequence == 3 ? presenter.title.data.at(rgba) : uint8_t(sequence);
    const uint8_t green = sequence == 3 ? presenter.title.data.at(rgba+1) : 37;
    const uint8_t blue = sequence == 3 ? presenter.title.data.at(rgba+2) : 83;
    if (uint8_t(bytes[at]) != blue || uint8_t(bytes[at+1]) != green || uint8_t(bytes[at+2]) != red)
      throw std::runtime_error("synthetic BMP payload mismatch");
  }
  Need(true, "every retained pixel checked against supplied synthetic image");
}
}
int main(int argc, char** argv) {
  std::jthread watchdog([](std::stop_token stop) {
    const auto deadline = std::chrono::steady_clock::now() + 60s;
    while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(10ms);
    if (!stop.stop_requested()) std::_Exit(4);
  });
  int exit_code = 1;
  try {
    Need(argc == 4, "SCENARIO FRESH_ABSOLUTE_ROOT PINNED_TITLE_BMP");
    const std::string scenario = argv[1];
    const std::set<std::string> scenarios{"success","nav-fail","before-fail","motion-fail","cancel-before",
        "cancel-after","phase-begin-fail","timer-begin-fail","timer-finish-fail","timer-write-fail",
        "phase-finish-fail","phase-write-fail","config-fail","terminal-write-fail","compound-fail"};
    Need(scenarios.contains(scenario), "exact one-shot scenario");
    const bool nav_ok = scenario != "nav-fail" && scenario != "cancel-before";
    const bool phase_begin_attempt = nav_ok && scenario != "before-fail";
    const bool phase_begun = phase_begin_attempt && scenario != "phase-begin-fail";
    const bool timer_begin_attempt = phase_begun;
    const bool timer_begun = timer_begin_attempt && scenario != "timer-begin-fail";
    const bool cancelled = scenario == "cancel-before" || scenario == "cancel-after";
    const bool motion_ok = timer_begun && !cancelled && scenario != "motion-fail";
    const bool timer_ok = timer_begun && !cancelled && scenario != "timer-finish-fail" &&
        scenario != "timer-write-fail" && scenario != "compound-fail";
    const bool phase_ok = phase_begun && !cancelled && scenario != "phase-finish-fail" &&
        scenario != "phase-write-fail" && scenario != "compound-fail";
    const bool config_ok = scenario != "config-fail" && scenario != "compound-fail";
    const bool terminal_ok = scenario != "terminal-write-fail";
    const bool success = nav_ok && motion_ok && timer_ok && phase_ok && config_ok && terminal_ok;
    const std::filesystem::path root = argv[2];
    Need(root.is_absolute() && root == root.lexically_normal() && !root.filename().empty() &&
        !std::filesystem::exists(root), "fresh absolute owned root only");
    auto parent = mcla::launcher::fs::OpenDirectory(root.parent_path().wstring());
    Need(parent.has_value(), "safe existing output parent");
    NoCapture loader; LoadTitle(loader, std::filesystem::path(argv[3]));
    Need(std::filesystem::create_directory(root), "create only supplied fresh child");
    binding_integration::main_thread = std::this_thread::get_id(); binding_integration::scenario = scenario;
    REXCVAR_SET(mcla_arcade_timer_probe, true); REXCVAR_SET(mcla_arcade_update_binding_probe, true);
    REXCVAR_SET(mcla_phase_census_probe, true); REXCVAR_SET(mcla_arcade_config_probe, true);
    REXCVAR_SET(mcla_autostart_probe, true); REXCVAR_SET(mcla_autostart_token, stub::token);
    REXCVAR_SET(mcla_arcade_navigation_probe, true); REXCVAR_SET(mcla_motion_calibration_probe, true);
    REXCVAR_SET(mcla_motion_config_probe, true); REXCVAR_SET(mcla_timer_config_probe, true);
    REXCVAR_SET(mcla_raw_config_experiment, false); REXCVAR_SET(mcla_fixed60_config_experiment, false);
    REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
    Need(Admits(), "actual stock binding admission"); stub::fail_freeze = !config_ok;
    a::Driver driver; Need(driver.Setup() == rex::X_STATUS{0}, "actual driver setup");
    BindingPresenter presenter; presenter.title = std::move(loader.synthetic_title);
    std::stop_source stop; binding_integration::cancellation = &stop;
    if (scenario == "cancel-before") stop.request_stop();
    std::vector<uint16_t> buttons; unsigned motion_edges = 0;
    uint16_t previous_button = 0; bool previous_motion = false;
    binding_integration::sample_input = [&] {
      rex::input::X_INPUT_STATE state{};
      Need(driver.GetState(0, &state) == rex::X_RESULT{0}, "synthetic controller read");
      const auto& pad = state.gamepad;
      Need((!pad.buttons || pad.buttons == 0x10 || pad.buttons == 0x200 || pad.buttons == 0x1000) &&
           (!pad.right_trigger || pad.right_trigger == 255) && !(pad.buttons && pad.right_trigger) &&
           !pad.left_trigger && !pad.thumb_lx && !pad.thumb_ly && !pad.thumb_rx && !pad.thumb_ry,
           "bounded internal buttons/RT255 only");
      if (pad.buttons && !previous_button) buttons.push_back(pad.buttons);
      if (pad.right_trigger && !previous_motion) ++motion_edges;
      previous_button = pad.buttons; previous_motion = pad.right_trigger != 0;
    };
    const bool returned = a::RunMotionConfig(stop.get_token(), presenter, driver, root);
    binding_integration::sample_input = {};
    using namespace binding_integration;
    Need(returned == success, "exact lifecycle return including terminal-write failure");
    Need(!contract_failed && !event_overflow && !forbidden_calls && !stub::installs && !stub::begins,
         "all stub order/window contracts; no Runtime or actual guest observer calls");
    Need(stub::freezes == 1 && stub::disarms == 1, "configuration always attempted/disarmed once");
    Need(phase_begins == unsigned(phase_begin_attempt) && timer_begins == unsigned(timer_begin_attempt) &&
         timer_finishes == unsigned(timer_begun && !cancelled) && phase_finishes == 1,
         "exact independent phase/timer Begin and Finish attempts");
    Need(timer_disarms == 1u + unsigned(!timer_ok) && phase_disarms == 1u + unsigned(!phase_ok),
         "failure plus unconditional terminal disarm counts");
    Need(write_failures == unsigned(scenario == "timer-write-fail" || scenario == "phase-write-fail" ||
         scenario == "terminal-write-fail"), "only exact selected owned WriteFile failure");
    Need(windows == (motion_ok ? 25u : 1u), "fixed timed-image window checks plus independent cut");
    const unsigned expected_captures = !nav_ok ? unsigned(scenario == "nav-fail") :
        motion_ok ? 43u : scenario == "motion-fail" ? 20u : 19u;
    Need(presenter.calls == expected_captures, "exact capture prefix, no retries");
    Need(buttons == (nav_ok ? std::vector<uint16_t>{0x10,0x10,0x200,0x1000,0x1000,0x1000,0x1000} :
                              std::vector<uint16_t>{}) && motion_edges == unsigned(timer_begun && !cancelled),
         "exact navigation edges and at most one motion edge");
    rex::input::X_INPUT_STATE neutral{};
    Need(driver.GetState(0, &neutral) == rex::X_RESULT{0} && !neutral.gamepad.buttons &&
         !neutral.gamepad.right_trigger && !driver.ArmMotion(), "terminal neutral; cannot rearm");
    auto event_at = [&](const char* name, bool last = false) {
      unsigned index = 0;
      for (unsigned i = 0; i < event_count; ++i) if (std::string_view(events[i].name) == name) {
        index = i + 1; if (!last) break;
      }
      return index;
    };
    const auto pf = event_at("phase-finish"), td = event_at("timer-disarm", true), pd = event_at("phase-disarm", true);
    Need(pf && td > pf && pd > td && events[td-1].config_freezes == 1 && events[pd-1].config_freezes == 1,
         "terminal timer/phase disarms occur after phase attempt and configuration Freeze");
    Need(events[pf-1].config_freezes == 0 && (event_at("timer-finish") || event_at("timer-disarm")),
         "phase attempt precedes configuration and follows timer terminal action");
    if (timer_begin_attempt) Need(event_at("phase-begin") < event_at("timer-begin"), "phase Begin strictly precedes timer Begin");
    if (config_ok) Need(event_at("config-write") > pf, "configuration write remains after sidecar attempts");
    Need(event_at("terminal-write") > pd, "terminal write attempted after unconditional all-disarm guard");

    const auto timer_text = Read(root / "motion-config/timer-calls.json");
    const auto phase_text = Read(root / "motion-config/phase-census.json");
    const auto config_text = Read(root / "motion-config/timer-config.json");
    const auto terminal_text = Read(root / "motion-config/result.json");
    Need(timer_text == (timer_ok ? std::string("{\"synthetic_timer_stub\":true,\"token\":\"") + stub::token + "\"}" : ""),
         "exact timer bytes or retained empty failed output");
    Need(phase_text == (phase_ok ? std::string("{\"synthetic_phase_stub\":true,\"token\":\"") + stub::token + "\"}" : ""),
         "exact phase bytes even when timer Begin/Finish/write failed");
    Need(config_text == (config_ok ? stub::retained : ""), "exact independent configuration bytes");
    const std::string timer_failure = !timer_begun ? "ARCADE_TIMER_WINDOW_UNAVAILABLE" : cancelled ? "ARCADE_TIMER_WAIT_FAILED" :
        scenario == "timer-finish-fail" || scenario == "compound-fail" ? "ARCADE_TIMER_FINISH_FAILED" :
        scenario == "timer-write-fail" ? "ARCADE_TIMER_WRITE_FAILED" : "";
    const std::string phase_failure = !phase_begun || cancelled || scenario == "phase-finish-fail" || scenario == "compound-fail" ?
        "ARCADE_PHASE_FINISH_FAILED" : scenario == "phase-write-fail" ? "ARCADE_PHASE_WRITE_FAILED" : "";
    const std::string failure = !config_ok ? "CONFIGURATION_CUT_OR_WRITE_FAILED" : !nav_ok ? "NAVIGATION_FAILED" :
        !motion_ok ? "MOTION_FAILED" : !timer_failure.empty() ? timer_failure : phase_failure;
    if (terminal_ok) {
      const Json wanted{{"schema","mcla-arcade-timer-terminal-v2"},{"token",stub::token},{"mode","stock"},
          {"status",success ? "COLLECTED_REVIEW_PENDING" : "FAILED"},{"failure",failure},
          {"stop_requested",cancelled},{"navigation_collected",nav_ok},{"motion_collected",motion_ok},
          {"configuration_written",config_ok},{"configuration_file","timer-config.json"},{"configuration_bytes",config_text.size()},
          {"timer_written",timer_ok},{"timer_file","timer-calls.json"},{"timer_bytes",timer_text.size()},
          {"timer_failure",timer_failure},{"timer_disarmed",true},{"before_frame_timer_covered",false},
          {"phase_written",phase_ok},{"phase_file","phase-census.json"},{"phase_bytes",phase_text.size()},
          {"phase_failure",phase_failure},{"phase_disarmed",true},{"before_frame_phase_covered",false},
          {"controller_disarmed",true},{"injection_requested",true},{"input_injected",nav_ok},{"os_input",false},
          {"desktop_capture",false},{"scene_proof",false},{"movement_proof",false},{"physical_speed_proof",false},
          {"fps_proof",false},{"configuration_accepted",false}};
      Need(Json::parse(terminal_text) == wanted, "whole exact v2 terminal incl failure precedence and no proof upgrades");
    } else Need(terminal_text.empty(), "failed terminal write preserved as empty, never invented receipt");
    std::set<std::filesystem::path> expected_files{"motion-config/result.json","motion-config/timer-config.json",
        "motion-config/timer-calls.json","motion-config/phase-census.json","autostart/result.json"};
    unsigned saved = 0;
    auto image = [&](const std::filesystem::path& name, const Json& provenance, unsigned sequence) {
      expected_files.insert(name); VerifyImage(root / name, provenance, sequence, presenter); ++saved;
    };
    const auto nav = Json::parse(Read(root / "autostart/result.json"));
    Need(nav.at("frames").size() == (nav_ok ? 6u : 0u) && nav.at("fps_proven") == false,
         "navigation success or failed empty prefix preserved");
    for (unsigned i = 0; i < nav.at("frames").size(); ++i)
      image(std::filesystem::path("autostart") / (i ? "post-" + std::to_string(i) + ".bmp" : "title.bmp"),
            nav.at("frames")[i], i+3);
    if (nav_ok) {
      expected_files.insert("arcade-navigation/result.json"); expected_files.insert("motion-calibration/result.json");
      const auto arcade = Json::parse(Read(root / "arcade-navigation/result.json"));
      const auto motion = Json::parse(Read(root / "motion-calibration/result.json"));
      Need(arcade.at("frames").size() == 10 && arcade.at("commands").size() == 6 && arcade.at("failure") == "",
           "complete Arcade prefix retained");
      for (unsigned i = 0; i < 10; ++i)
        image(std::filesystem::path("arcade-navigation") / ("frame-" + std::to_string(i) + ".bmp"),
              arcade.at("frames")[i].at("provenance"), i+9);
      Need(motion.at("frames").size() == (motion_ok ? 25u : scenario == "motion-fail" ? 2u : 1u) &&
           motion.at("capture_retry_count") == 0 && motion.at("fps_proven") == false, "exact motion prefix without retries/proof");
      for (unsigned i = 0; i < motion.at("frames").size(); ++i) {
        const auto& row = motion.at("frames")[i]; Need(row.at("slot") == i, "all motion slots retained in order");
        if (row.at("saved") == true)
          image(std::filesystem::path("motion-calibration") / (i ? "motion-" + std::to_string(i) + ".bmp" : "before.bmp"),
                row.at("provenance"), i+19);
        else Need((scenario == "before-fail" && i == 0) || (scenario == "motion-fail" && i == 1),
                  "only explicit failed capture has no pixels");
      }
    }
    Need(saved == (!nav_ok ? 0u : motion_ok ? 41u : scenario == "before-fail" ? 16u : 17u), "exact saved BMP count");
    std::set<std::filesystem::path> actual_files;
    for (const auto& item : std::filesystem::recursive_directory_iterator(root)) {
      Need(!(GetFileAttributesW(item.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT), "no output reparse points");
      if (item.is_directory()) continue;
      Need(item.is_regular_file(), "only regular artifacts"); actual_files.insert(item.path().lexically_relative(root));
    }
    Need(actual_files == expected_files, "exact complete owned artifact inventory");
    std::cout << "ARCADE_BINDING_INTEGRATION scenario=" << scenario << " checks=" << checks
              << " failures=0 saved_bmps=" << saved << " artifacts=" << actual_files.size()
              << " synthetic_ms=" << milliseconds.load() << " STUB_LIFECYCLE_SYNTHETIC_PIXELS_ONLY\n";
    std::cout.flush(); std::cerr.flush(); exit_code = std::cout.good() && std::cerr.good() ? 0 : 2;
  } catch (const std::exception& e) {
    binding_integration::sample_input = {};
    std::cerr << "FAIL " << e.what() << '\n'; std::cerr.flush(); exit_code = std::cerr.good() ? 1 : 2;
  }
  watchdog.request_stop(); watchdog.join(); return exit_code;
}
