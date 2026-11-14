// Actual autostart.cpp, synthetic QPC/Pause/presenter, and explicit timer/phase/
// clock/history/config STUBS. NOT actual SDK/Clock/Observer integration proof.
// No Runtime object, game, GPU, window, OS input, desktop capture, explicit SDK
// load, or actual SDK clock/witness call. Linked SDK support is not a mock SDK.
// One fresh process: SCENARIO FRESH_ABSOLUTE_ROOT PINNED_TITLE_BMP.
// Link launcher_fs_win.cpp and the same retained SDK support as the binding
// fixture; DO NOT link arcade_timer/clock_game/controller implementations.
// Required private compile guards: AUTOSTART, MOTION_CALIBRATION, MOTION_CONFIG,
// TIMER_CONFIG, RAW_CONFIG_EXPERIMENT, FIXED60_CONFIG_EXPERIMENT,
// GUEST_SOURCE_SCOUT, UPDATE_TIMING, INPUT_RETURN, ARCADE_TIMER,
// ARCADE_UPDATE_BINDING, PHASE_CENSUS, CLOCK_READ_WITNESS,
// CLOCK_COMMIT_HISTORY (usual suffixes). Missing-API RED not executed here.
// Copied/derived seams, not an include of another test's main or Registry.
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
#include <vector>

namespace history_integration {
constexpr const char* token = "e69a68cd28e641f5a1e8e92d720f1068";
constexpr DWORD partial_bytes = 17;
std::atomic<uint64_t> milliseconds{1000};
std::thread::id main_thread;
std::function<void()> sample_input;
std::string scenario;
std::array<const char*, 96> events{};
unsigned event_count{}, forbidden{}, timer_begins{}, timer_finishes{}, timer_disarms{};
unsigned phase_begins{}, phase_finishes{}, phase_disarms{}, config_freezes{}, config_disarms{};
unsigned clock_begins{}, clock_finishes{}, clock_disarms{}, synthetic_sdk_stops{}, write_failures{};
unsigned history_begins{}, history_finishes{}, synthetic_history_stops{}, flush_failures{};
bool contract_failed{}, event_overflow{}, timer_begun{}, timer_returned{}, timer_stopped{}, phase_begun{}, phase_stopped{};
bool clock_begun{}, history_begun{}, shared_host_cut{}, history_stopped{};
uint64_t before_qpc{}, phase_begin{}, phase_end{}, phase_frequency{};
std::string clock_text, history_text, timer_text, phase_text, config_text;
bool HistoryFinishFails() { return scenario == "history-finish-pending" || scenario == "history-finish-reject" ||
    scenario == "both-fail" || scenario == "precedence-history-phase" || scenario == "precedence-history-config"; }
void Event(const char* name) noexcept {
  if (event_count == events.size()) { event_overflow = true; return; }
  events[event_count++] = name;
}
void Contract(bool value) {
  if (!value) { contract_failed = true; throw std::runtime_error("synthetic lifecycle contract violation"); }
}
uint64_t Tick() { return milliseconds.load(std::memory_order_acquire) * 10000 + 1; }
BOOL WINAPI Counter(LARGE_INTEGER* out) { out->QuadPart = static_cast<LONGLONG>(Tick()); return TRUE; }
BOOL WINAPI Frequency(LARGE_INTEGER* out) { out->QuadPart = 10000000; return TRUE; }
BOOL WINAPI Write(HANDLE file, LPCVOID data, DWORD bytes, LPDWORD written, LPOVERLAPPED overlap) {
  if (bytes != 54 && bytes != 3840) { // Fixed synthetic BMP header/rows.
    wchar_t path[32768]{};
    const DWORD n = GetFinalPathNameByHandleW(file, path, 32768, FILE_NAME_NORMALIZED);
    if (n && n < 32768) {
      const std::wstring name(path, n);
      const bool timer = name.ends_with(L"\\motion-config\\timer-calls.json");
      const bool clock = name.ends_with(L"\\motion-config\\clock-reads.json");
      const bool history = name.ends_with(L"\\motion-config\\clock-history.json");
      const bool phase = name.ends_with(L"\\motion-config\\phase-census.json");
      if (timer) Event("timer-write");
      if (clock) Event("clock-write");
      if (history) Event("history-write");
      if (phase) Event("phase-write");
      if (name.ends_with(L"\\motion-config\\timer-config.json")) Event("config-write");
      if (name.ends_with(L"\\motion-config\\result.json")) Event("terminal-write");
      if ((timer && scenario == "timer-write-fail") ||
          (phase && (scenario == "phase-write-fail" || scenario == "precedence-history-phase")) ||
          (clock && (scenario == "clock-write-zero" || scenario == "clock-write-partial")) ||
          (history && (scenario == "history-write-zero" || scenario == "history-write-partial"))) {
        ++write_failures;
        if ((clock && scenario == "clock-write-partial") || (history && scenario == "history-write-partial")) {
          Contract(bytes > partial_bytes && written && !overlap);
          DWORD actual = 0;
          Contract(::WriteFile(file, data, partial_bytes, &actual, nullptr) && actual == partial_bytes);
          *written = actual; // Actual prefix exists, then FALSE reports its count.
        } else if (written) *written = 0;
        SetLastError(ERROR_WRITE_FAULT); return FALSE;
      }
    }
  }
  return ::WriteFile(file, data, bytes, written, overlap);
}
BOOL WINAPI Flush(HANDLE file) {
  wchar_t path[32768]{};
  const DWORD n = GetFinalPathNameByHandleW(file, path, 32768, FILE_NAME_NORMALIZED);
  if (n && n < 32768 && std::wstring_view(path,n).ends_with(L"\\motion-config\\clock-history.json")) {
    Event("history-flush");
    if (scenario == "history-flush-fail") {
      ++flush_failures; SetLastError(ERROR_WRITE_FAULT); return FALSE;
    }
  }
  return ::FlushFileBuffers(file);
}
}
#define MCLA_AUTOSTART_TEST_CLOCK 1
#define QueryPerformanceCounter history_integration::Counter
#define QueryPerformanceFrequency history_integration::Frequency
#define WriteFile history_integration::Write
#define FlushFileBuffers history_integration::Flush
#include "mcla_autostart_win.cpp"
#undef FlushFileBuffers
#undef WriteFile
#undef QueryPerformanceFrequency
#undef QueryPerformanceCounter
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <set>

#if !defined(MCLA_CLOCK_COMMIT_HISTORY_PROBE) || !defined(MCLA_CLOCK_READ_WITNESS_PROBE) || !defined(MCLA_ARCADE_UPDATE_BINDING_PROBE) || \
    !defined(MCLA_PHASE_CENSUS_PROBE) || !defined(MCLA_ARCADE_TIMER_PROBE) || \
    !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || \
    !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || \
    !defined(MCLA_UPDATE_TIMING_PROBE)
#error Compile all explicit clock/binding and legacy admission branches.
#endif
#define STUB_FLAG(name) REXCVAR_DEFINE_BOOL(name, false, "Test", "Synthetic clock lifecycle fixture only") \
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
STUB_FLAG(mcla_clock_read_witness_probe);
STUB_FLAG(mcla_clock_history_probe);
STUB_FLAG(mcla_arcade_timer_probe);
STUB_FLAG(mcla_arcade_update_binding_probe);
STUB_FLAG(mcla_phase_census_probe);
STUB_FLAG(mcla_timer_config_probe);
STUB_FLAG(mcla_fixed60_config_experiment);
STUB_FLAG(mcla_raw_config_experiment);
STUB_FLAG(mcla_source_scout_probe);
STUB_FLAG(mcla_update_timing_probe);
STUB_FLAG(mcla_motion_unknown_probe);
STUB_FLAG(mcla_motion_unknown_experiment);
#undef STUB_FLAG
REXCVAR_DEFINE_STRING(mcla_source_scout_token, "", "Test", "Unused stationary token");
namespace mcla::guest_source_scout {
bool Enabled() { return REXCVAR_GET(mcla_source_scout_probe); }
void ValidateExclusiveConfig(std::string_view) { throw std::runtime_error("no stationary scout"); }
}
namespace mcla::autostart_test_clock {
uint64_t NowMs() { return history_integration::milliseconds.load(std::memory_order_acquire); }
bool Pause(std::stop_token stop, uint64_t ms) {
  if (std::this_thread::get_id() != history_integration::main_thread) {
    std::this_thread::yield(); return !stop.stop_requested();
  }
  if (stop.stop_requested()) return false;
  if (ms > 150000 || NowMs() > 200000) throw std::runtime_error("synthetic clock budget");
  if (history_integration::sample_input) history_integration::sample_input();
  while (ms && !stop.stop_requested()) {
    const auto step = std::min<uint64_t>(ms, 10);
    history_integration::milliseconds.fetch_add(step, std::memory_order_release); ms -= step;
    if (history_integration::sample_input) history_integration::sample_input();
  }
  return !stop.stop_requested();
}
}
namespace mcla::input_return {
bool Enabled() { return false; }
void Enable() { ++history_integration::forbidden; throw std::runtime_error("no census"); }
void Install(rex::Runtime&) { ++history_integration::forbidden; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++history_integration::forbidden; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++history_integration::forbidden; throw std::runtime_error("no census freeze"); }
void Disarm() noexcept { ++history_integration::forbidden; }
}
namespace mcla::timer_config {
bool Enabled() { return REXCVAR_GET(mcla_timer_config_probe); }
bool ExperimentEnabled() { return REXCVAR_GET(mcla_fixed60_config_experiment); }
bool RawExperimentEnabled() { return REXCVAR_GET(mcla_raw_config_experiment); }
void ValidateExperimentSelection() {
  if (ExperimentEnabled() && RawExperimentEnabled()) throw std::runtime_error("mutually exclusive selection");
}
void Install(rex::Runtime&) { ++history_integration::forbidden; throw std::runtime_error("no Runtime config install"); }
void Begin(uint64_t, uint64_t, uint64_t, const std::string&) {
  ++history_integration::forbidden; throw std::runtime_error("no Runtime config Begin");
}
std::string Freeze(const std::string& token) {
  using namespace history_integration;
  ++config_freezes; Event("config-freeze"); Contract(token == history_integration::token && phase_finishes == 1);
  if (scenario == "config-fail" || scenario == "precedence-history-config") throw std::runtime_error("synthetic configuration cut failure");
  config_text = std::string("{\"synthetic_config_stub\":true,\"token\":\"") + token + "\"}";
  return config_text;
}
void Disarm() noexcept { ++history_integration::config_disarms; history_integration::Event("config-disarm"); }
}
namespace history_integration { mcla::timer_journal::Window window; }
namespace mcla::clock_game {
void Install() { ++history_integration::forbidden; throw std::runtime_error("no actual clock install"); }
void InstallWithHistory() { ++history_integration::forbidden; throw std::runtime_error("no actual history install"); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace history_integration;
  ++clock_begins; Event("clock-begin");
  Contract(!clock_begun && timer_begins == 1 && !timer_begun && phase_begun &&
      begin == phase_begin && end == phase_end && frequency == phase_frequency && token == history_integration::token);
  clock_begun = true;
  if (arcade_timer::ClockHistoryEnabled()) { ++history_begins; history_begun = true; Event("history-begin"); }
}
uint64_t Read(PPCContext&, uint8_t*) { ++history_integration::forbidden; throw std::runtime_error("no actual clock read"); }
void Invoke(PPCFunc*, PPCContext&, uint8_t*, const clock_read_witness::host_journal::Invocation&) {
  ++history_integration::forbidden; throw std::runtime_error("no actual original invocation");
}
std::string FinishAfterTimerDrain(const std::string& token) {
  using namespace history_integration;
  ++clock_finishes; Event("clock-finish");
  Contract(clock_begun && timer_returned && timer_stopped && !phase_stopped && !config_freezes &&
      token == history_integration::token);
  shared_host_cut = true; Event("synthetic-host-freeze");
  ++synthetic_sdk_stops; Event("synthetic-sdk-stop"); // STUB, never actual SDK Stop.
  if (arcade_timer::ClockHistoryEnabled()) {
    Contract(history_begun && !history_stopped);
    ++synthetic_history_stops; history_stopped = true; Event("synthetic-history-stop");
  }
  Event("clock-snapshot"); // Both simulated Stops precede even a failed V2 payload.
  if (scenario == "clock-finish-fail" || scenario == "both-fail") throw std::runtime_error("synthetic clock snapshot failure after both stops");
  clock_text = std::string("{\"synthetic_clock_stub\":true,\"token\":\"") + token +
      "\",\"complete_clock_history_proven\":false,\"fps_proven\":false}";
  return clock_text;
}
std::string FinishHistoryAfterTimerDrain(const std::string& token) {
  using namespace history_integration;
  ++history_finishes; Event("history-finish");
  Contract(arcade_timer::ClockHistoryEnabled() && history_begun && history_stopped && shared_host_cut &&
      clock_finishes == 1 && synthetic_sdk_stops == 1 && synthetic_history_stops == 1 &&
      timer_returned && !phase_stopped && !config_freezes && token == history_integration::token);
  // This is a lifecycle/file literal, NOT the real4096-row history schema or
  // a parser/SDK pending reproduction. The real controller has separate tests.
  if (HistoryFinishFails()) throw std::runtime_error(scenario == "history-finish-pending" ?
      "synthetic history PENDING after shared cut" : "synthetic history rejected payload");
  history_text = std::string("{\"synthetic_history_stub\":true,\"token\":\"") + token +
      "\",\"interval_kind\":\"source-routed-lexical\",\"complete_clock_history_proven\":false,\"fps_proven\":false}";
  return history_text;
}
void Disarm() noexcept { ++history_integration::clock_disarms; history_integration::Event("clock-disarm"); }
}
namespace mcla::arcade_timer {
bool Enabled() { return REXCVAR_GET(mcla_arcade_timer_probe); }
bool UpdateBindingEnabled() { return REXCVAR_GET(mcla_arcade_update_binding_probe); }
bool ClockWitnessEnabled() { return REXCVAR_GET(mcla_clock_read_witness_probe); }
bool ClockHistoryEnabled() { return REXCVAR_GET(mcla_clock_history_probe); }
void Install(rex::Runtime&) { ++history_integration::forbidden; throw std::runtime_error("no Runtime timer install"); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace history_integration;
  ++timer_begins; Event("timer-begin");
  Contract(!timer_begun && phase_begun && !phase_stopped && token == history_integration::token &&
      begin == phase_begin && end == phase_end && frequency == phase_frequency && begin == Tick());
  // Explicit stub for the separately tested production timer Begin coupling.
  if (ClockWitnessEnabled()) clock_game::Begin(begin, end, frequency, token);
  window = {begin, end, frequency}; timer_begun = true;
}
timer_journal::Window ObservationWindow(const std::string& token) {
  history_integration::Contract(history_integration::timer_begun && token == history_integration::token);
  return history_integration::window;
}
std::string Finish(std::stop_token stop, const std::string& token) {
  using namespace history_integration;
  ++timer_finishes; Event("timer-finish");
  Contract(timer_begun && !timer_stopped && !phase_stopped && !stop.stop_requested() &&
      token == history_integration::token && Tick() >= window.end_qpc && !config_freezes);
  timer_stopped = true;
  if (scenario == "timer-finish-fail") { Event("timer-throw"); throw std::runtime_error("synthetic timer drain failure"); }
  timer_text = std::string("{\"synthetic_timer_stub\":true,\"token\":\"") + token + "\"}";
  timer_returned = true; Event("timer-return"); return timer_text;
}
void Disarm() noexcept {
  ++history_integration::timer_disarms; history_integration::timer_stopped = true;
  history_integration::Event("timer-disarm");
  if (ClockWitnessEnabled()) clock_game::Disarm(); // Mirror wrapper's terminal-only coupling.
}
void Invoke(PPCFunc*, PPCContext&, uint8_t*) { ++history_integration::forbidden; throw std::runtime_error("no timer original"); }
}
namespace mcla::phase_census {
bool Enabled() { return REXCVAR_GET(mcla_phase_census_probe); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace history_integration;
  ++phase_begins; Event("phase-begin");
  Contract(!phase_begun && !timer_begins && !phase_stopped && token == history_integration::token &&
      frequency == 10000000 && begin == Tick() && end == begin + 12 * frequency && before_qpc && begin >= before_qpc);
  phase_begin = begin; phase_end = end; phase_frequency = frequency; phase_begun = true;
}
std::string Finish(std::stop_token stop, const std::string& token) {
  using namespace history_integration;
  ++phase_finishes; Event("phase-finish");
  Contract(phase_begun && timer_stopped && !phase_stopped && !config_freezes && !stop.stop_requested() &&
      token == history_integration::token);
  phase_stopped = true;
  if (scenario == "phase-finish-fail") throw std::runtime_error("synthetic phase finish failure");
  phase_text = std::string("{\"synthetic_phase_stub\":true,\"token\":\"") + token + "\"}";
  return phase_text;
}
void Disarm() noexcept {
  ++history_integration::phase_disarms; history_integration::phase_stopped = true; history_integration::Event("phase-disarm");
}
}
namespace {
namespace a = mcla::autostart;
using Json = nlohmann::json;
unsigned checks{};
void Need(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
std::string Read(const std::filesystem::path& path, uint64_t maximum = 65536) {
  const auto size = std::filesystem::file_size(path); Need(size <= maximum, "bounded test input/output");
  std::ifstream file(path, std::ios::binary); std::string text(size, '\0');
  file.read(text.data(), static_cast<std::streamsize>(size));
  Need(file.good() || (file.eof() && size_t(file.gcount()) == size), "complete test bytes"); return text;
}
bool Admits() { try { a::ValidateConfig(); return true; } catch (...) { return false; } }
void Configure(bool clock, bool history) {
  REXCVAR_SET(mcla_clock_read_witness_probe, clock);
  REXCVAR_SET(mcla_clock_history_probe, history);
  REXCVAR_SET(mcla_arcade_timer_probe, true); REXCVAR_SET(mcla_arcade_update_binding_probe, true);
  REXCVAR_SET(mcla_phase_census_probe, true); REXCVAR_SET(mcla_arcade_config_probe, true);
  REXCVAR_SET(mcla_autostart_probe, true); REXCVAR_SET(mcla_autostart_token, history_integration::token);
  REXCVAR_SET(mcla_arcade_navigation_probe, true); REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_motion_config_probe, true); REXCVAR_SET(mcla_timer_config_probe, true);
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
}
class SyntheticPresenter final : public rex::ui::Presenter {
 public:
  SyntheticPresenter() : Presenter({}) {}
  rex::ui::RawImage title;
  unsigned calls{};
  rex::ui::Surface::TypeFlags GetSupportedSurfaceTypes() const override { return 0; }
  bool CaptureGuestOutput(rex::ui::RawImage&) override { throw std::runtime_error("wrong capture API"); }
  rex::ui::GuestCaptureStatus CaptureGuestOutputWithProvenance(rex::ui::GuestOutputCapture& out) override {
    ++calls; Need(!title.data.empty() && calls <= 43, "bounded synthetic publications"); out = {};
    if (calls <= 3) out.image = title;
    else {
      out.image.width = 1280; out.image.height = 720; out.image.stride = 5120;
      out.image.data.resize(size_t(1280) * 720 * 4);
      for (size_t i = 0; i < out.image.data.size(); i += 4) {
        out.image.data[i] = uint8_t(calls); out.image.data[i+1] = 37;
        out.image.data[i+2] = 83; out.image.data[i+3] = 255;
      }
    }
    const auto tick = history_integration::Tick(); auto& p = out.provenance;
    p.presenter_epoch = 1; p.acquired_sequence = calls; p.qpc_frequency = 10000000;
    p.refresh_cpu_begin_qpc = p.refresh_cpu_end_qpc = p.acquire_before_qpc = p.acquire_after_qpc =
        p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = tick;
    if (calls == 19) history_integration::before_qpc = tick;
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
void LoadTitle(SyntheticPresenter& presenter, const std::filesystem::path& path) {
  Need(path.is_absolute() && std::filesystem::is_regular_file(path), "explicit staged read-only title");
  const auto bytes = Read(path, 4 * 1024 * 1024);
  auto le = [&](size_t at, unsigned count) {
    uint32_t value = 0; for (unsigned i = 0; i < count; ++i) value |= uint32_t(uint8_t(bytes.at(at+i))) << (8*i);
    return value;
  };
  constexpr size_t payload = size_t(1280) * 720 * 3;
  Need(bytes.size() == 54 + payload && le(0,2) == 0x4d42 && le(2,4) == bytes.size() && le(10,4) == 54 &&
      le(14,4) == 40 && le(18,4) == 1280 && le(22,4) == 720 && le(26,2) == 1 && le(28,2) == 24 &&
      le(30,4) == 0 && le(34,4) == payload, "exact bounded 720p title BMP transport");
  auto& image = presenter.title; image.width = 1280; image.height = 720; image.stride = 5120;
  image.data.resize(size_t(image.stride) * image.height);
  for (uint32_t y = 0; y < 720; ++y) for (uint32_t x = 0; x < 1280; ++x) {
    const size_t source = 54 + (size_t(719-y)*1280+x)*3, target = size_t(y)*image.stride + size_t(x)*4;
    for (unsigned c = 0; c < 3; ++c) image.data[target+c] = uint8_t(bytes[source+2-c]);
    image.data[target+3] = 255;
  }
  Need(mcla_stationary_title::Classify(image) == mcla_stationary_title::Frame::Title,
       "production title classifier accepts pinned synthetic input");
}
unsigned EventAt(const char* name, bool last = false) {
  unsigned found = 0;
  for (unsigned i = 0; i < history_integration::event_count; ++i)
    if (std::string_view(history_integration::events[i]) == name) { found = i+1; if (!last) break; }
  return found;
}
void Invalid(const std::string& scenario) {
  Need(Admits(), "complete ON configuration first admitted");
  if (scenario == "invalid-timer") REXCVAR_SET(mcla_arcade_timer_probe, false);
  else if (scenario == "invalid-clock") REXCVAR_SET(mcla_clock_read_witness_probe, false);
  else if (scenario == "invalid-binding") REXCVAR_SET(mcla_arcade_update_binding_probe, false);
  else if (scenario == "invalid-phase") REXCVAR_SET(mcla_phase_census_probe, false);
  else if (scenario == "invalid-config") REXCVAR_SET(mcla_arcade_config_probe, false);
  else if (scenario == "invalid-motion-config") REXCVAR_SET(mcla_motion_config_probe, false);
  else if (scenario == "invalid-timer-config") REXCVAR_SET(mcla_timer_config_probe, false);
  else if (scenario == "invalid-fixed") REXCVAR_SET(mcla_fixed60_config_experiment, true);
  else if (scenario == "invalid-raw") REXCVAR_SET(mcla_raw_config_experiment, true);
  else if (scenario == "invalid-unknown-probe") REXCVAR_SET(mcla_motion_unknown_probe, true);
  else if (scenario == "invalid-unknown-experiment") REXCVAR_SET(mcla_motion_unknown_experiment, true);
  else throw std::runtime_error("unknown invalid scenario");
  Need(!Admits(), "clock opt-in never bypasses dependencies/stock/exclusive flags");
  Need(!history_integration::event_count && !history_integration::forbidden,
       "pure validation never starts/stops controllers or SDK");
}
}
int main(int argc, char** argv) {
  using namespace std::chrono_literals;
  std::jthread watchdog([](std::stop_token stop) {
    const auto deadline = std::chrono::steady_clock::now() + 60s;
    while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(10ms);
    if (!stop.stop_requested()) std::_Exit(4);
  });
  int exit_code = 1;
  try {
    Need(argc == 4, "SCENARIO FRESH_ABSOLUTE_ROOT PINNED_TITLE_BMP required");
    using namespace history_integration;
    scenario = argv[1];
    const std::set<std::string> scenarios{"success", "runtime-off", "clock-only", "timer-finish-fail", "timer-write-fail",
        "clock-finish-fail", "clock-write-zero", "clock-write-partial", "phase-write-fail", "config-fail",
        "history-finish-pending", "history-finish-reject", "history-write-zero", "history-write-partial", "history-flush-fail",
        "both-fail", "phase-finish-fail", "precedence-history-phase", "precedence-history-config", "invalid-clock",
        "invalid-timer", "invalid-binding", "invalid-phase", "invalid-config", "invalid-motion-config",
        "invalid-timer-config", "invalid-fixed", "invalid-raw",
        "invalid-unknown-probe", "invalid-unknown-experiment"};
    Need(scenarios.contains(scenario), "exact one-shot scenario");
    const auto* flag = rex::cvar::GetFlagInfo("mcla_clock_read_witness_probe");
    Need(flag && flag->default_value == "false" && flag->getter && flag->getter() == "false" &&
        flag->lifecycle == rex::cvar::Lifecycle::kInitOnly && Admits(), "clock opt-in default OFF init-only");
    const auto* hflag = rex::cvar::GetFlagInfo("mcla_clock_history_probe");
    Need(hflag && hflag->default_value == "false" && hflag->getter && hflag->getter() == "false" &&
        hflag->lifecycle == rex::cvar::Lifecycle::kInitOnly, "history opt-in default OFF init-only");
    const bool clock = scenario != "runtime-off";
    const bool history = clock && scenario != "clock-only";
    Configure(clock, history);
    const std::filesystem::path root = argv[2];
    Need(root.is_absolute() && root == root.lexically_normal() && !root.filename().empty() &&
        !std::filesystem::exists(root), "fresh absolute root only; never reuse/delete");
    auto parent = mcla::launcher::fs::OpenDirectory(root.parent_path().wstring());
    Need(parent.has_value(), "safe existing parent");
    if (scenario.starts_with("invalid-")) {
      Invalid(scenario); Need(!std::filesystem::exists(root), "admission-only case writes no output");
    } else {
      Need(Admits(), "actual stock configuration admission ON or OFF");
      SyntheticPresenter presenter; LoadTitle(presenter, std::filesystem::path(argv[3]));
      Need(std::filesystem::create_directory(root), "create only explicit fresh child");
      main_thread = std::this_thread::get_id(); a::Driver driver;
      Need(driver.Setup() == rex::X_STATUS{0}, "actual internal driver setup");
      unsigned motion_edges = 0; bool previous_motion = false; uint16_t previous_button = 0;
      std::vector<uint16_t> buttons;
      sample_input = [&] {
        rex::input::X_INPUT_STATE state{}; Need(driver.GetState(0, &state) == rex::X_RESULT{0}, "internal synthetic read");
        const auto& pad = state.gamepad;
        Need((!pad.buttons || pad.buttons == 0x10 || pad.buttons == 0x200 || pad.buttons == 0x1000) &&
            (!pad.right_trigger || pad.right_trigger == 255) && !(pad.buttons && pad.right_trigger) &&
            !pad.left_trigger && !pad.thumb_lx && !pad.thumb_ly && !pad.thumb_rx && !pad.thumb_ry,
            "bounded internal buttons/RT255 only; no OS injection");
        if (pad.buttons && !previous_button) buttons.push_back(pad.buttons);
        if (pad.right_trigger && !previous_motion) ++motion_edges;
        previous_button = pad.buttons; previous_motion = pad.right_trigger != 0;
      };
      const bool returned = a::RunMotionConfig(std::stop_token{}, presenter, driver, root); sample_input = {};
      const bool drain = scenario != "timer-finish-fail";
      const bool timer_written = drain && scenario != "timer-write-fail";
      const bool clock_finished = clock && drain;
      const bool clock_payload = clock_finished && scenario != "clock-finish-fail" && scenario != "both-fail";
      const bool clock_written = clock_payload && scenario != "clock-write-zero" && scenario != "clock-write-partial";
      const bool history_finished = history && drain;
      const bool history_payload = history_finished && !HistoryFinishFails();
      const bool history_written = history_payload && scenario != "history-write-zero" && scenario != "history-write-partial" &&
          scenario != "history-flush-fail";
      const bool phase_payload = scenario != "phase-finish-fail";
      const bool phase_written = phase_payload && scenario != "phase-write-fail" && scenario != "precedence-history-phase";
      const bool config_written = scenario != "config-fail" && scenario != "precedence-history-config";
      const bool success = timer_written && (!clock || clock_written) && (!history || history_written) && phase_written && config_written;
      Need(returned == success && !contract_failed && !event_overflow && !forbidden,
           "exact workflow result; all stub contracts; no native hook/Runtime calls");
      Need(timer_begins == 1 && timer_finishes == 1 && phase_begins == 1 && phase_finishes == 1 &&
          config_freezes == 1 && config_disarms == 1, "all independent cuts attempted exactly once");
      Need(clock_begins == unsigned(clock) && clock_finishes == unsigned(clock_finished) &&
          synthetic_sdk_stops == unsigned(clock_finished), "clock Finish/SDK-stop stub only after timer RETURN");
      Need(history_begins == unsigned(history) && history_finishes == unsigned(history_finished) &&
          synthetic_history_stops == unsigned(history_finished), "history shared-cut/accessor stub only after timer RETURN");
      Need((clock ? clock_disarms >= 1 : clock_disarms == 0) && timer_disarms >= 1 && phase_disarms >= 1,
           "cleanup is terminal Disarm, no clock API activity OFF");
      Need(presenter.calls == 43 && buttons == std::vector<uint16_t>{0x10,0x10,0x200,0x1000,0x1000,0x1000,0x1000} &&
          motion_edges == 1, "fixed capture/navigation/motion schedule without retries");
      Need(EventAt("phase-begin") < EventAt("timer-begin") && EventAt("timer-finish") < EventAt("phase-finish") &&
          EventAt("phase-finish") < EventAt("config-freeze") &&
          EventAt("config-disarm") < EventAt("terminal-write"), "independent cut/cleanup/terminal event order");
      if (drain) Need(EventAt("timer-return") && EventAt("timer-return") < EventAt("timer-write"),
                      "timer drain attestation exists only after successful return");
      else Need(!EventAt("timer-return") && !EventAt("clock-finish") && !EventAt("history-finish") &&
          !EventAt("synthetic-sdk-stop") && !EventAt("synthetic-history-stop"),
          "failed timer Finish never guesses drain or stops either SDK family");
      if (clock_finished) Need(EventAt("timer-return") < EventAt("clock-finish") &&
          EventAt("timer-write") < EventAt("clock-finish") && EventAt("clock-finish") < EventAt("phase-finish"),
          "separate clock cut follows successful return even after timer write throws");
      if (history_finished) {
        Need(EventAt("synthetic-host-freeze") && EventAt("synthetic-sdk-stop") && EventAt("synthetic-history-stop") &&
            EventAt("clock-snapshot") && EventAt("history-finish") &&
            EventAt("synthetic-host-freeze") < EventAt("synthetic-sdk-stop") &&
            EventAt("synthetic-sdk-stop") < EventAt("clock-snapshot") &&
            EventAt("synthetic-history-stop") < EventAt("clock-snapshot") &&
            EventAt("clock-snapshot") < EventAt("history-finish") && EventAt("history-finish") < EventAt("phase-finish"),
            "shared host cut and both simulated Stops precede independent V2 and history payloads");
        if (EventAt("clock-write")) Need(EventAt("clock-write") < EventAt("history-finish"),
            "history cut still follows attempted V2 write even when that write fails");
      }
      Need(EventAt("timer-disarm", true) > EventAt("config-freeze") &&
          EventAt("phase-disarm", true) > EventAt("config-freeze"), "unconditional cleanup after all cuts");
      const auto timer_bytes = Read(root / "motion-config/timer-calls.json");
      const auto phase_bytes = Read(root / "motion-config/phase-census.json");
      const auto config_bytes = Read(root / "motion-config/timer-config.json");
      Need(timer_bytes == (timer_written ? timer_text : "") && phase_bytes == (phase_written ? phase_text : "") &&
          config_bytes == (config_written ? config_text : ""), "exact retained independent sidecar bytes");
      const auto actual_clock = clock ? Read(root / "motion-config/clock-reads.json", 24ull * 1024 * 1024) : "";
      const auto expected_actual = scenario == "clock-write-partial" ? clock_text.substr(0, partial_bytes) :
          clock_written ? clock_text : "";
      Need(actual_clock == expected_actual, "exact complete/empty/physical partial clock bytes, never invented JSON");
      const auto actual_history = history ? Read(root / "motion-config/clock-history.json", 16ull * 1024 * 1024) : "";
      const auto expected_history = scenario == "history-write-partial" ? history_text.substr(0, partial_bytes) :
          (history_written || scenario == "history-flush-fail") ? history_text : "";
      Need(actual_history == expected_history, "exact history complete/empty/partial/full-byte-flush-failure physical payload");
      const std::string timer_failure = !drain ? "ARCADE_TIMER_FINISH_FAILED" :
          !timer_written ? "ARCADE_TIMER_WRITE_FAILED" : "";
      const std::string phase_failure = !phase_payload ? "ARCADE_PHASE_FINISH_FAILED" : phase_written ? "" : "ARCADE_PHASE_WRITE_FAILED";
      const std::string clock_failure = !drain ? "CLOCK_TIMER_DRAIN_NOT_PROVEN" :
          !clock_payload ? "CLOCK_FINISH_FAILED" : !clock_written ? "CLOCK_WRITE_FAILED" : "";
      const std::string history_failure = !drain ? "HISTORY_TIMER_DRAIN_NOT_PROVEN" :
          !history_payload ? "HISTORY_FINISH_FAILED" : !history_written ? "HISTORY_WRITE_FAILED" : "";
      const std::string failure = !config_written ? "CONFIGURATION_CUT_OR_WRITE_FAILED" :
          !timer_failure.empty() ? timer_failure : !phase_failure.empty() ? phase_failure :
          clock && !clock_failure.empty() ? clock_failure : history ? history_failure : "";
      Json expected{{"schema",history ? "mcla-arcade-clock-history-terminal-v1" :
          clock ? "mcla-arcade-clock-terminal-v1" : "mcla-arcade-timer-terminal-v2"},
          {"token",token},{"mode","stock"},{"status",success ? "COLLECTED_REVIEW_PENDING" : "FAILED"},
          {"failure",failure},{"stop_requested",false},{"navigation_collected",true},{"motion_collected",true},
          {"configuration_written",config_written},{"configuration_file","timer-config.json"},{"configuration_bytes",config_bytes.size()},
          {"timer_written",timer_written},{"timer_file","timer-calls.json"},{"timer_bytes",timer_bytes.size()},
          {"timer_failure",timer_failure},{"timer_disarmed",true},{"before_frame_timer_covered",false},
          {"phase_written",phase_written},{"phase_file","phase-census.json"},{"phase_bytes",phase_bytes.size()},
          {"phase_failure",phase_failure},{"phase_disarmed",true},{"before_frame_phase_covered",false},
          {"controller_disarmed",true},{"injection_requested",true},{"input_injected",true},{"os_input",false},
          {"desktop_capture",false},{"scene_proof",false},{"movement_proof",false},{"physical_speed_proof",false},
          {"fps_proof",false},{"configuration_accepted",false}};
      if (clock) {
        expected["clock_required"] = true; expected["clock_written"] = clock_written;
        expected["clock_file"] = "clock-reads.json"; expected["clock_bytes"] = clock_payload ? clock_text.size() : 0;
        expected["clock_actual_bytes"] = actual_clock.size(); expected["clock_failure"] = clock_failure;
        expected["clock_timer_drained"] = drain; expected["clock_disarmed"] = true;
        expected["before_frame_clock_covered"] = false;
      }
      if (history) {
        expected["history_required"] = true; expected["history_written"] = history_written;
        expected["history_file"] = "clock-history.json"; expected["history_bytes"] = history_payload ? history_text.size() : 0;
        expected["history_actual_bytes"] = actual_history.size(); expected["history_failure"] = history_failure;
        expected["history_timer_drained"] = drain; expected["history_disarmed"] = true;
        expected["before_frame_history_covered"] = false;
        expected["history_interval_kind"] = "source-routed-lexical";
        expected["history_exact_timer_wall_interval_proven"] = false;
        expected["history_universal_writer_coverage_proven"] = false;
      }
      const auto terminal = Json::parse(Read(root / "motion-config/result.json"));
      Need(terminal == expected, "whole exact terminal:12H/9C fields, byte counts, independent failures and false proofs");
      Need(write_failures == unsigned(scenario == "timer-write-fail" || scenario == "phase-write-fail" ||
          scenario == "precedence-history-phase" || scenario == "clock-write-zero" || scenario == "clock-write-partial" ||
          scenario == "history-write-zero" || scenario == "history-write-partial"), "only one selected owned write failure");
      Need(flush_failures == unsigned(scenario == "history-flush-fail"), "only selected owned full-byte flush failure");
      if (scenario == "history-flush-fail") Need(!history_written && terminal.at("history_bytes") == terminal.at("history_actual_bytes") &&
          actual_history == history_text && terminal.at("status") == "FAILED", "all bytes reported is not a successful history write or durability proof");
      if (!clock) {
        Need(!std::filesystem::exists(root / "motion-config/clock-reads.json") && !EventAt("clock-write") &&
            !EventAt("clock-finish") && !EventAt("clock-begin"), "runtime OFF retains old schema/fields/no extra file or clock lifecycle");
      }
      if (!history) Need(!std::filesystem::exists(root / "motion-config/clock-history.json") &&
          !EventAt("history-write") && !EventAt("history-finish") && !EventAt("synthetic-history-stop"),
          "legacy O/C schemas keep history disabled with no new file or lifecycle");
      const auto nav = Json::parse(Read(root / "autostart/result.json"));
      const auto arcade = Json::parse(Read(root / "arcade-navigation/result.json"));
      const auto motion = Json::parse(Read(root / "motion-calibration/result.json"));
      Need(nav.at("fps_proven") == false && arcade.at("frames").size() == 10 && arcade.at("commands").size() == 6 &&
          motion.at("frames").size() == 25 && motion.at("capture_retry_count") == 0 && motion.at("fps_proven") == false,
          "synthetic scene prefixes retained without FPS/scene promotion");
      std::set<std::filesystem::path> expected_files{"motion-config/result.json", "motion-config/timer-config.json",
          "motion-config/timer-calls.json", "motion-config/phase-census.json", "autostart/result.json",
          "arcade-navigation/result.json", "motion-calibration/result.json"};
      if (clock) expected_files.insert("motion-config/clock-reads.json");
      if (history) expected_files.insert("motion-config/clock-history.json");
      for (unsigned i = 0; i < 6; ++i) expected_files.insert(std::filesystem::path("autostart") /
          (i ? "post-" + std::to_string(i) + ".bmp" : "title.bmp"));
      for (unsigned i = 0; i < 10; ++i) expected_files.insert(std::filesystem::path("arcade-navigation") /
          ("frame-" + std::to_string(i) + ".bmp"));
      for (unsigned i = 0; i < 25; ++i) expected_files.insert(std::filesystem::path("motion-calibration") /
          (i ? "motion-" + std::to_string(i) + ".bmp" : "before.bmp"));
      std::set<std::filesystem::path> actual_files;
      for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        Need(!(GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT), "no output reparse points");
        if (entry.is_directory()) continue;
        Need(entry.is_regular_file(), "only regular owned output files");
        actual_files.insert(entry.path().lexically_relative(root));
      }
      Need(actual_files == expected_files && actual_files.size() == (history ? 50u : clock ? 49u : 48u),
          "exact complete owned artifact inventory:50H/49C/48O");
      rex::input::X_INPUT_STATE neutral{};
      Need(driver.GetState(0, &neutral) == rex::X_RESULT{0} && !neutral.gamepad.buttons &&
          !neutral.gamepad.right_trigger && !driver.ArmMotion(), "terminal remains neutral and cannot rearm");
    }
    std::cout << "ARCADE_HISTORY_INTEGRATION scenario=" << scenario << " checks=" << checks
              << " failures=0 STUB_LIFECYCLE_SYNTHETIC_PIXELS_ONLY\n";
    std::cout.flush(); std::cerr.flush(); exit_code = std::cout.good() && std::cerr.good() ? 0 : 2;
  } catch (const std::exception& e) {
    history_integration::sample_input = {};
    std::cerr << "FAIL " << e.what() << '\n'; std::cerr.flush(); exit_code = std::cerr.good() ? 1 : 2;
  }
  watchdog.request_stop(); watchdog.join(); return exit_code;
}


