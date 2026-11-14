// Actual autostart.cpp, synthetic QPC/Pause/presenter, and explicit timer/phase/
// clock/history/config/swap-leaf STUBS. NOT SDK swap/Clock/Observer proof.
// No Runtime object, game, GPU, window, OS input, desktop capture, explicit SDK
// load, or actual SDK clock/witness call. Linked SDK support is not a mock SDK.
// One fresh process: SCENARIO FRESH_ABSOLUTE_ROOT PINNED_TITLE_BMP.
// Link launcher_fs_win.cpp and the same retained SDK support as the binding
// fixture; DO NOT link arcade_timer/clock_game/controller implementations.
// Required private compile guards: AUTOSTART, MOTION_CALIBRATION, MOTION_CONFIG,
// TIMER_CONFIG, RAW_CONFIG_EXPERIMENT, FIXED60_CONFIG_EXPERIMENT,
// GUEST_SOURCE_SCOUT, UPDATE_TIMING, INPUT_RETURN, ARCADE_TIMER,
// ARCADE_UPDATE_BINDING, PHASE_CENSUS, CLOCK_READ_WITNESS,
// CLOCK_COMMIT_HISTORY, ARCADE_SWAP_BINDING, SWAP_OBSERVER (usual suffixes),
// and REXGLUE_SWAP_EMISSION_WITNESS. Leaf implementation
// is NOT linked: another fixture covers its paired-SDK behavior. No execution here.
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

namespace swap_binding_integration {
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
unsigned swap_begins{}, swap_records{}, swap_serializes{}, swap_stop_calls{}, synthetic_issuer_stops{};
unsigned latest_presenter_call{}, first_motion_capture_call{};
std::string swap_text;
bool contract_failed{}, event_overflow{}, timer_begun{}, timer_returned{}, timer_stopped{}, phase_begun{}, phase_stopped{};
bool clock_begun{}, history_begun{}, shared_host_cut{}, history_stopped{};
uint64_t before_qpc{}, phase_begin{}, phase_end{}, phase_frequency{};
std::string clock_text, history_text, timer_text, phase_text, config_text;
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
  if ((bytes != 54 && bytes != 3840) ||
      (bytes && data && static_cast<const char*>(data)[0] == '{')) { // Do not mistake same-size JSON for a BMP row.
    wchar_t path[32768]{};
    const DWORD n = GetFinalPathNameByHandleW(file, path, 32768, FILE_NAME_NORMALIZED);
    if (n && n < 32768) {
      const std::wstring name(path, n);
      const bool timer = name.ends_with(L"\\motion-config\\timer-calls.json");
      const bool clock = name.ends_with(L"\\motion-config\\clock-reads.json");
      const bool history = name.ends_with(L"\\motion-config\\clock-history.json");
      const bool swap = name.ends_with(L"\\motion-config\\swap-source.json");
      const bool phase = name.ends_with(L"\\motion-config\\phase-census.json");
      if (timer) Event("timer-write");
      if (clock) Event("clock-write");
      if (history) Event("history-write");
      if (swap) Event("swap-write");
      if (phase) Event("phase-write");
      if (name.ends_with(L"\\motion-config\\timer-config.json")) Event("config-write");
      if (name.ends_with(L"\\motion-config\\result.json")) Event("terminal-write");
      if ((timer && scenario == "timer-write-fail") ||
          (phase && (scenario == "phase-write-fail" || scenario == "precedence-history-phase" || scenario == "precedence-swap-phase")) ||
          (clock && (scenario == "clock-write-zero" || scenario == "clock-write-partial")) ||
          (history && (scenario == "history-write-zero" || scenario == "history-write-partial")) ||
          (swap && (scenario == "swap-write-zero" || scenario == "swap-write-partial"))) {
        ++write_failures;
        if ((clock && scenario == "clock-write-partial") || (history && scenario == "history-write-partial") ||
            (swap && scenario == "swap-write-partial")) {
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
  if (n && n < 32768 && std::wstring_view(path,n).ends_with(L"\\motion-config\\swap-source.json")) {
    Event("swap-flush");
    if (scenario == "swap-flush-fail") {
      ++flush_failures; SetLastError(ERROR_WRITE_FAULT); return FALSE;
    }
  }
  return ::FlushFileBuffers(file);
}
}
#define MCLA_AUTOSTART_TEST_CLOCK 1
#define QueryPerformanceCounter swap_binding_integration::Counter
#define QueryPerformanceFrequency swap_binding_integration::Frequency
#define WriteFile swap_binding_integration::Write
#define FlushFileBuffers swap_binding_integration::Flush
#include "mcla_autostart_win.cpp"
#undef FlushFileBuffers
#undef WriteFile
#undef QueryPerformanceFrequency
#undef QueryPerformanceCounter
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <set>

namespace swap_binding_integration {
using Provenance = rex::ui::GuestCaptureProvenance;
mcla::arcade_swap_binding::Attempt swap_attempt;
std::array<Provenance, 64> publications{};
std::array<Provenance, 25> received{};
std::array<bool, 25> received_present{};
const Provenance* current_publication{};
bool SameBinding(const rex::diagnostics::swap_emission::Binding& a,
                 const rex::diagnostics::swap_emission::Binding& b) {
  return a.result == b.result && a.match.token == b.match.token &&
      a.match.snapshot == b.match.snapshot && a.match.snapshot_result == b.match.snapshot_result &&
      a.stock == b.stock && a.fetch == b.fetch && a.consume_qpc == b.consume_qpc;
}
bool SameProvenance(const Provenance& a, const Provenance& b) {
  return a.presenter_epoch == b.presenter_epoch && a.acquired_sequence == b.acquired_sequence &&
      a.qpc_frequency == b.qpc_frequency && a.refresh_cpu_begin_qpc == b.refresh_cpu_begin_qpc &&
      a.refresh_cpu_end_qpc == b.refresh_cpu_end_qpc && a.acquire_before_qpc == b.acquire_before_qpc &&
      a.acquire_after_qpc == b.acquire_after_qpc && a.copy_submit_before_qpc == b.copy_submit_before_qpc &&
      a.copy_completion_observed_qpc == b.copy_completion_observed_qpc &&
      a.pixels_ready_qpc == b.pixels_ready_qpc && SameBinding(a.swap_emission, b.swap_emission);
}
nlohmann::json ProvenanceJson(const Provenance& p) {
  const auto& b = p.swap_emission; const auto& m = b.match; const auto& z = m.snapshot;
  return {{"presenter_epoch",p.presenter_epoch},{"acquired_sequence",p.acquired_sequence},
      {"qpc_frequency",p.qpc_frequency},{"refresh_cpu_begin_qpc",p.refresh_cpu_begin_qpc},
      {"refresh_cpu_end_qpc",p.refresh_cpu_end_qpc},{"acquire_before_qpc",p.acquire_before_qpc},
      {"acquire_after_qpc",p.acquire_after_qpc},{"copy_submit_before_qpc",p.copy_submit_before_qpc},
      {"copy_completion_observed_qpc",p.copy_completion_observed_qpc},{"pixels_ready_qpc",p.pixels_ready_qpc},
      {"binding",{{"result",static_cast<uint32_t>(b.result)},{"stock",b.stock},{"fetch",b.fetch},
       {"consume_qpc",b.consume_qpc},{"token_epoch",m.token.epoch},{"token_serial",m.token.serial},
       {"snapshot_result",static_cast<uint32_t>(m.snapshot_result)},
       {"snapshot",{{"qpc_before",z.qpc_before},{"qpc_after",z.qpc_after},
        {"native_thread_id",z.native_thread_id},{"guest_caller_lr",z.guest_caller_lr},
        {"timer_address",z.timer_address},{"timer_counter",z.timer_counter},
        {"timer_delta_bits",z.timer_delta_bits},{"snapshot_available",z.snapshot_available},
        {"callback_id",z.callback_id}}}}}};
}
void SyntheticBinding(Provenance& p, unsigned call) {
  namespace sw = rex::diagnostics::swap_emission;
  auto& b = p.swap_emission;
  for (size_t i=0;i<b.stock.size();++i) b.stock[i] = 0xA0000000u + call*16 + uint32_t(i);
  for (size_t i=0;i<b.fetch.size();++i) b.fetch[i] = 0xB0000000u + call*16 + uint32_t(i);
  b.match.token = {0xE69A68CD28E641F5ull,call}; b.consume_qpc = p.refresh_cpu_begin_qpc;
  if (call%3 == 1) {
    b.result=sw::Result::kTokenMatched; b.match.snapshot_result=sw::SnapshotResult::kAvailable;
    auto& z=b.match.snapshot; z.qpc_before=z.qpc_after=p.refresh_cpu_begin_qpc;
    z.native_thread_id=17;z.guest_caller_lr=0x8241A0E8;z.timer_address=0x827D7500;
    z.timer_counter=call;z.timer_delta_bits=0x3D088889;z.snapshot_available=true;z.callback_id=100+call;
  } else if (call%3 == 2) {
    b.result=sw::Result::kTokenMatched;b.match.snapshot_result=sw::SnapshotResult::kUnavailable;
  } else {
    // Invalid/raw fields intentionally survive; not an actual SDK emission.
    b.result=static_cast<sw::Result>(100+call);
    b.match.snapshot_result=static_cast<sw::SnapshotResult>(200+call);
    b.match.snapshot.callback_id=UINT64_MAX-call;b.match.snapshot.timer_counter=call;
  }
}
}

#if !defined(MCLA_ARCADE_SWAP_BINDING_PROBE) || !defined(MCLA_SWAP_OBSERVER_PROBE) || \
    !defined(REXGLUE_SWAP_EMISSION_WITNESS) || !defined(MCLA_CLOCK_COMMIT_HISTORY_PROBE) || \
    !defined(MCLA_CLOCK_READ_WITNESS_PROBE) || !defined(MCLA_ARCADE_UPDATE_BINDING_PROBE) || \
    !defined(MCLA_PHASE_CENSUS_PROBE) || !defined(MCLA_ARCADE_TIMER_PROBE) || \
    !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || \
    !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || \
    !defined(MCLA_UPDATE_TIMING_PROBE)
#error Compile all explicit clock/binding and legacy admission branches.
#endif
#define STUB_FLAG(name) REXCVAR_DEFINE_BOOL(name, false, "Test", "Synthetic clock lifecycle fixture only") \
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
STUB_FLAG(mcla_arcade_swap_binding_probe);
STUB_FLAG(mcla_swap_observer_probe);
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
uint64_t NowMs() { return swap_binding_integration::milliseconds.load(std::memory_order_acquire); }
bool Pause(std::stop_token stop, uint64_t ms) {
  if (std::this_thread::get_id() != swap_binding_integration::main_thread) {
    std::this_thread::yield(); return !stop.stop_requested();
  }
  if (stop.stop_requested()) return false;
  if (ms > 150000 || NowMs() > 200000) throw std::runtime_error("synthetic clock budget");
  if (swap_binding_integration::sample_input) swap_binding_integration::sample_input();
  while (ms && !stop.stop_requested()) {
    const auto step = std::min<uint64_t>(ms, 10);
    swap_binding_integration::milliseconds.fetch_add(step, std::memory_order_release); ms -= step;
    if (swap_binding_integration::sample_input) swap_binding_integration::sample_input();
  }
  return !stop.stop_requested();
}
}
namespace mcla::input_return {
bool Enabled() { return false; }
void Enable() { ++swap_binding_integration::forbidden; throw std::runtime_error("no census"); }
void Install(rex::Runtime&) { ++swap_binding_integration::forbidden; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++swap_binding_integration::forbidden; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++swap_binding_integration::forbidden; throw std::runtime_error("no census freeze"); }
void Disarm() noexcept { ++swap_binding_integration::forbidden; }
}
namespace mcla::timer_config {
bool Enabled() { return REXCVAR_GET(mcla_timer_config_probe); }
bool ExperimentEnabled() { return REXCVAR_GET(mcla_fixed60_config_experiment); }
bool RawExperimentEnabled() { return REXCVAR_GET(mcla_raw_config_experiment); }
void ValidateExperimentSelection() {
  if (ExperimentEnabled() && RawExperimentEnabled()) throw std::runtime_error("mutually exclusive selection");
}
void Install(rex::Runtime&) { ++swap_binding_integration::forbidden; throw std::runtime_error("no Runtime config install"); }
void Begin(uint64_t, uint64_t, uint64_t, const std::string&) {
  ++swap_binding_integration::forbidden; throw std::runtime_error("no Runtime config Begin");
}
std::string Freeze(const std::string& token) {
  using namespace swap_binding_integration;
  ++config_freezes; Event("config-freeze"); Contract(token == swap_binding_integration::token && phase_finishes == 1);
  if (scenario == "config-fail" || scenario == "precedence-history-config" || scenario == "precedence-swap-config") throw std::runtime_error("synthetic configuration cut failure");
  config_text = std::string("{\"synthetic_config_stub\":true,\"token\":\"") + token + "\"}";
  return config_text;
}
void Disarm() noexcept { ++swap_binding_integration::config_disarms; swap_binding_integration::Event("config-disarm"); }
}
namespace swap_binding_integration { mcla::timer_journal::Window window; }
namespace mcla::clock_game {
void Install() { ++swap_binding_integration::forbidden; throw std::runtime_error("no actual clock install"); }
void InstallWithHistory() { ++swap_binding_integration::forbidden; throw std::runtime_error("no history install"); }
void Begin(uint64_t, uint64_t, uint64_t, const std::string&) {
  ++swap_binding_integration::forbidden; throw std::runtime_error("clock must remain OFF");
}
uint64_t Read(PPCContext&, uint8_t*) { ++swap_binding_integration::forbidden; throw std::runtime_error("no clock read"); }
void Invoke(PPCFunc*, PPCContext&, uint8_t*, const clock_read_witness::host_journal::Invocation&) {
  ++swap_binding_integration::forbidden; throw std::runtime_error("no clock original");
}
std::string FinishAfterTimerDrain(const std::string&) {
  ++swap_binding_integration::forbidden; throw std::runtime_error("no clock cut");
}
std::string FinishHistoryAfterTimerDrain(const std::string&) {
  ++swap_binding_integration::forbidden; throw std::runtime_error("no history cut");
}
void Disarm() noexcept { ++swap_binding_integration::forbidden; }
}
namespace mcla::arcade_timer {
bool Enabled() { return REXCVAR_GET(mcla_arcade_timer_probe); }
bool UpdateBindingEnabled() { return REXCVAR_GET(mcla_arcade_update_binding_probe); }
bool ClockWitnessEnabled() { return REXCVAR_GET(mcla_clock_read_witness_probe); }
bool ClockHistoryEnabled() { return REXCVAR_GET(mcla_clock_history_probe); }
void Install(rex::Runtime&) { ++swap_binding_integration::forbidden; throw std::runtime_error("no Runtime timer install"); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace swap_binding_integration;
  ++timer_begins; Event("timer-begin");
  Contract(!timer_begun && phase_begun && !phase_stopped && token == swap_binding_integration::token &&
      begin == phase_begin && end == phase_end && frequency == phase_frequency && begin == Tick());
  // Explicit stub for the separately tested production timer Begin coupling.
  if (ClockWitnessEnabled()) clock_game::Begin(begin, end, frequency, token);
  window = {begin, end, frequency}; timer_begun = true;
}
timer_journal::Window ObservationWindow(const std::string& token) {
  swap_binding_integration::Contract(swap_binding_integration::timer_begun && token == swap_binding_integration::token);
  return swap_binding_integration::window;
}
std::string Finish(std::stop_token stop, const std::string& token) {
  using namespace swap_binding_integration;
  ++timer_finishes; Event("timer-finish");
  Contract(timer_begun && !timer_stopped && !phase_stopped && !stop.stop_requested() &&
      token == swap_binding_integration::token && Tick() >= window.end_qpc && !config_freezes);
  timer_stopped = true;
  if (scenario == "timer-finish-fail") { Event("timer-throw"); throw std::runtime_error("synthetic timer drain failure"); }
  timer_text = std::string("{\"synthetic_timer_stub\":true,\"token\":\"") + token + "\"}";
  timer_returned = true; Event("timer-return"); return timer_text;
}
void Disarm() noexcept {
  ++swap_binding_integration::timer_disarms; swap_binding_integration::timer_stopped = true;
  swap_binding_integration::Event("timer-disarm");
  if (ClockWitnessEnabled()) clock_game::Disarm(); // Mirror wrapper's terminal-only coupling.
}
void Invoke(PPCFunc*, PPCContext&, uint8_t*) { ++swap_binding_integration::forbidden; throw std::runtime_error("no timer original"); }
}
namespace mcla::phase_census {
bool Enabled() { return REXCVAR_GET(mcla_phase_census_probe); }
void Begin(uint64_t begin, uint64_t end, uint64_t frequency, const std::string& token) {
  using namespace swap_binding_integration;
  ++phase_begins; Event("phase-begin");
  Contract(!phase_begun && !timer_begins && !phase_stopped && token == swap_binding_integration::token &&
      frequency == 10000000 && begin == Tick() && end == begin + 12 * frequency && before_qpc && begin >= before_qpc);
  phase_begin = begin; phase_end = end; phase_frequency = frequency; phase_begun = true;
}
std::string Finish(std::stop_token stop, const std::string& token) {
  using namespace swap_binding_integration;
  ++phase_finishes; Event("phase-finish");
  Contract(phase_begun && timer_stopped && !phase_stopped && !config_freezes && !stop.stop_requested() &&
      token == swap_binding_integration::token);
  if (arcade_swap_binding::Enabled()) Contract(swap_serializes == 1 && !config_freezes);
  phase_stopped = true;
  if (scenario == "phase-finish-fail") throw std::runtime_error("synthetic phase finish failure");
  phase_text = std::string("{\"synthetic_phase_stub\":true,\"token\":\"") + token + "\"}";
  return phase_text;
}
void Disarm() noexcept {
  ++swap_binding_integration::phase_disarms; swap_binding_integration::phase_stopped = true; swap_binding_integration::Event("phase-disarm");
}
}

namespace mcla::arcade_swap_binding {
// Deliberate STUB leaf. No real sw::Configure/Emit/Consume or SDK observation.
bool Enabled() { return REXCVAR_GET(mcla_arcade_swap_binding_probe); }
const char* ReasonName(Reason r) noexcept {
  return r==Reason::Configured ? "CONFIGURED" : r==Reason::SdkRejected ? "SDK_REJECTED" : "NOT_ATTEMPTED";
}
bool BeginSharedWindow(uint64_t b,uint64_t e,uint64_t f,const std::string& t) noexcept {
  using namespace swap_binding_integration;
  if (swap_attempt.attempted || swap_attempt.serialize_attempted) return false;
  ++swap_begins;Event("swap-begin");swap_attempt.attempted=true;
  swap_attempt.begin_qpc=b;swap_attempt.end_qpc=e;swap_attempt.qpc_frequency=f;
  swap_attempt.token_valid=t==token;swap_attempt.epoch=0xE69A68CD28E641F5ull;
  if (!(Enabled() && phase_begun && timer_begun && phase_begins==1 && timer_begins==1 &&
        !phase_stopped && !timer_stopped && t==token && b==phase_begin && e==phase_end &&
        f==phase_frequency && e-b==12*f && swap_attempt.source_captures==1 &&
        received_present[0] && !swap_attempt.issuance_stop_requested)) {
    contract_failed=true;return false;
  }
  swap_attempt.configured=scenario!="swap-config-reject";
  swap_attempt.reason=swap_attempt.configured ? Reason::Configured : Reason::SdkRejected;
  return swap_attempt.configured;
}
void RecordCapture(size_t slot,const rex::ui::GuestCaptureProvenance& p) {
  using namespace swap_binding_integration;
  ++swap_records;
  Contract(Enabled() && !swap_attempt.serialize_attempted && slot<25 && !received_present[slot] &&
      slot==swap_attempt.source_captures && &p==current_publication &&
      latest_presenter_call>0 && latest_presenter_call<=publications.size() &&
      SameProvenance(p,publications[latest_presenter_call-1]));
  if (!slot) {
    Contract(!swap_attempt.attempted && !phase_begins && !timer_begins);
    first_motion_capture_call=latest_presenter_call;before_qpc=p.pixels_ready_qpc;Event("swap-before");
  } else Contract(swap_attempt.configured && latest_presenter_call==first_motion_capture_call+slot);
  if (scenario=="swap-record-throw" && slot==5) throw std::runtime_error("synthetic RecordCapture exception");
  received[slot]=p;received_present[slot]=true;++swap_attempt.source_captures;
}
void StopIssuing() noexcept {
  using namespace swap_binding_integration;++swap_stop_calls;Event("swap-stop-call");
  if (!swap_attempt.issuance_stop_requested) {
    Event("swap-stop");swap_attempt.issuance_stop_requested=true;
    if (swap_attempt.configured) ++synthetic_issuer_stops;
  }
}
Attempt ReadAttempt() noexcept { return swap_binding_integration::swap_attempt; }
std::string Serialize(const std::string& t) {
  using namespace swap_binding_integration;
  Contract(Enabled() && !swap_attempt.serialize_attempted && t==token &&
      timer_stopped && !phase_stopped && !phase_finishes && !config_freezes);
  ++swap_serializes;Event("swap-serialize");swap_attempt.serialize_attempted=true;StopIssuing();
  if (scenario=="swap-serialize-throw" || scenario=="precedence-swap-phase" || scenario=="precedence-swap-config")
    throw std::runtime_error("synthetic swap serialization exception");
  nlohmann::json rows=nlohmann::json::array();
  for (size_t i=0;i<25;++i)
    rows.push_back({{"slot",i},{"source",received_present[i] ? ProvenanceJson(received[i]) : nlohmann::json(nullptr)}});
  const nlohmann::json value{{"synthetic_swap_stub",true},{"token",t},
      {"source_captures",swap_attempt.source_captures},{"records",rows},
      {"statistics_quiescent",false},{"source_to_callback_association_proven",false},{"fps_proven",false}};
  swap_text=value.dump();Contract(swap_text.size()<128*1024);return swap_text;
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
void Configure(bool swap) {
  REXCVAR_SET(mcla_arcade_swap_binding_probe, swap);
  REXCVAR_SET(mcla_clock_read_witness_probe, false);
  REXCVAR_SET(mcla_clock_history_probe, false);
  REXCVAR_SET(mcla_arcade_timer_probe, true); REXCVAR_SET(mcla_arcade_update_binding_probe, true);
  REXCVAR_SET(mcla_phase_census_probe, true); REXCVAR_SET(mcla_arcade_config_probe, true);
  REXCVAR_SET(mcla_autostart_probe, true); REXCVAR_SET(mcla_autostart_token, swap_binding_integration::token);
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
    const auto tick = swap_binding_integration::Tick(); auto& p = out.provenance;
    p.presenter_epoch = 1; p.acquired_sequence = calls; p.qpc_frequency = 10000000;
    p.refresh_cpu_begin_qpc = p.refresh_cpu_end_qpc = p.acquire_before_qpc = p.acquire_after_qpc =
        p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = tick;
    if (!mcla::arcade_swap_binding::Enabled() && calls == 19)
      swap_binding_integration::before_qpc = tick; // Retained, already-tested legacy OFF fixture seam only.
    swap_binding_integration::SyntheticBinding(p,calls);
    swap_binding_integration::latest_presenter_call=calls;
    swap_binding_integration::current_publication=&p;
    swap_binding_integration::publications.at(calls-1)=p;
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
  for (unsigned i = 0; i < swap_binding_integration::event_count; ++i)
    if (std::string_view(swap_binding_integration::events[i]) == name) { found = i+1; if (!last) break; }
  return found;
}

void Invalid(const std::string& name) {
  Need(Admits(),"complete stock swap configuration first admitted");
  if (name=="invalid-clock") REXCVAR_SET(mcla_clock_read_witness_probe,true);
  else if (name=="invalid-history") {
    REXCVAR_SET(mcla_clock_read_witness_probe,true);REXCVAR_SET(mcla_clock_history_probe,true);
  } else if (name=="invalid-raw") REXCVAR_SET(mcla_raw_config_experiment,true);
  else if (name=="invalid-fixed") REXCVAR_SET(mcla_fixed60_config_experiment,true);
  else if (name=="invalid-sparse") REXCVAR_SET(mcla_arcade_sparse_capture,true);
  else if (name=="invalid-trigger") REXCVAR_SET(mcla_motion_calibration_trigger,1u);
  else if (name=="invalid-scout") REXCVAR_SET(mcla_source_scout_probe,true);
  else if (name=="invalid-old-swap") REXCVAR_SET(mcla_swap_observer_probe,true);
  else if (name=="invalid-timer") REXCVAR_SET(mcla_arcade_timer_probe,false);
  else if (name=="invalid-binding") REXCVAR_SET(mcla_arcade_update_binding_probe,false);
  else if (name=="invalid-phase") REXCVAR_SET(mcla_phase_census_probe,false);
  else if (name=="invalid-config") REXCVAR_SET(mcla_arcade_config_probe,false);
  else if (name=="invalid-unknown-probe") REXCVAR_SET(mcla_motion_unknown_probe,true);
  else throw std::runtime_error("unknown admission scenario");
  Need(!Admits(),"new swap opt-in never enables incompatible or missing dependencies");
  Need(!swap_binding_integration::event_count && !swap_binding_integration::forbidden,
       "admission rejection never starts a collector or SDK");
}
Json CaptureObject(const rex::ui::GuestCaptureProvenance& p) {
  return {{"width",1280},{"height",720},{"stride",5120},
      {"presenter_epoch",p.presenter_epoch},{"acquired_sequence",p.acquired_sequence},
      {"qpc_frequency",p.qpc_frequency},{"refresh_cpu_begin_qpc",p.refresh_cpu_begin_qpc},
      {"refresh_cpu_end_qpc",p.refresh_cpu_end_qpc},{"acquire_before_qpc",p.acquire_before_qpc},
      {"acquire_after_qpc",p.acquire_after_qpc},{"copy_submit_before_qpc",p.copy_submit_before_qpc},
      {"copy_completion_observed_qpc",p.copy_completion_observed_qpc},{"pixels_ready_qpc",p.pixels_ready_qpc}};
}
std::string LegacyCaptureBytes(const rex::ui::GuestCaptureProvenance& p) {
  // Explicit pre-extension key order; not the new leaf's serializer.
  return "{\"width\":1280,\"height\":720,\"stride\":5120,\"presenter_epoch\":"+std::to_string(p.presenter_epoch)+
      ",\"acquired_sequence\":"+std::to_string(p.acquired_sequence)+",\"qpc_frequency\":"+std::to_string(p.qpc_frequency)+
      ",\"refresh_cpu_begin_qpc\":"+std::to_string(p.refresh_cpu_begin_qpc)+",\"refresh_cpu_end_qpc\":"+std::to_string(p.refresh_cpu_end_qpc)+
      ",\"acquire_before_qpc\":"+std::to_string(p.acquire_before_qpc)+",\"acquire_after_qpc\":"+std::to_string(p.acquire_after_qpc)+
      ",\"copy_submit_before_qpc\":"+std::to_string(p.copy_submit_before_qpc)+
      ",\"copy_completion_observed_qpc\":"+std::to_string(p.copy_completion_observed_qpc)+
      ",\"pixels_ready_qpc\":"+std::to_string(p.pixels_ready_qpc)+"}";
}
}
int main(int argc,char** argv) {
  using namespace std::chrono_literals;
  std::jthread watchdog([](std::stop_token stop) {
    const auto end=std::chrono::steady_clock::now()+60s;
    while (!stop.stop_requested() && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(10ms);
    if (!stop.stop_requested()) std::_Exit(4);
  });
  int exit_code=1;
  try {
    using namespace swap_binding_integration;
    Need(argc==4,"SCENARIO FRESH_ABSOLUTE_ROOT PINNED_TITLE_BMP required");
    scenario=argv[1];
    const std::set<std::string> modes{"success","runtime-off","swap-config-reject","swap-record-throw",
        "swap-serialize-throw","swap-write-zero","swap-write-partial","swap-flush-fail",
        "timer-finish-fail","timer-write-fail","phase-finish-fail","phase-write-fail","config-fail",
        "precedence-swap-phase","precedence-swap-config","invalid-clock","invalid-history","invalid-raw",
        "invalid-fixed","invalid-sparse","invalid-trigger","invalid-scout","invalid-old-swap",
        "invalid-timer","invalid-binding","invalid-phase","invalid-config","invalid-unknown-probe"};
    Need(modes.contains(scenario),"exact fresh-process mode");
    const auto* flag=rex::cvar::GetFlagInfo("mcla_arcade_swap_binding_probe");
    Need(flag && flag->default_value=="false" && flag->getter && flag->getter()=="false" &&
         flag->lifecycle==rex::cvar::Lifecycle::kInitOnly && Admits(),"new leaf flag default OFF/init-only");
    const bool swap=scenario!="runtime-off";Configure(swap);
    const std::filesystem::path root=argv[2];
    Need(root.is_absolute() && root==root.lexically_normal() && !root.filename().empty() &&
         !std::filesystem::exists(root),"fresh absolute child only, never reuse/delete");
    auto parent=mcla::launcher::fs::OpenDirectory(root.parent_path().wstring());
    Need(parent.has_value(),"safe existing parent");
    if (scenario.starts_with("invalid-")) {
      Invalid(scenario);Need(!std::filesystem::exists(root),"pure admission rejection creates no output");
    } else {
      Need(Admits(),"exact stock dense admission");
      SyntheticPresenter presenter;LoadTitle(presenter,std::filesystem::path(argv[3]));
      Need(std::filesystem::create_directory(root),"create only owned fresh fixture root");
      main_thread=std::this_thread::get_id();a::Driver driver;
      Need(driver.Setup()==rex::X_STATUS{0},"actual internal diagnostic driver setup");
      unsigned motion_edges=0;bool previous_motion=false;uint16_t previous_button=0;
      std::vector<uint16_t> buttons;
      sample_input=[&] {
        rex::input::X_INPUT_STATE s{};Need(driver.GetState(0,&s)==rex::X_RESULT{0},"synthetic driver read");
        const auto& g=s.gamepad;
        Need((!g.buttons || g.buttons==0x10 || g.buttons==0x200 || g.buttons==0x1000) &&
             (!g.right_trigger || g.right_trigger==255) && !(g.buttons && g.right_trigger) &&
             !g.left_trigger && !g.thumb_lx && !g.thumb_ly && !g.thumb_rx && !g.thumb_ry,
             "bounded internal buttons/RT255 only, never OS input");
        if (g.buttons && !previous_button) buttons.push_back(g.buttons);
        if (g.right_trigger && !previous_motion) ++motion_edges;
        previous_button=g.buttons;previous_motion=g.right_trigger!=0;
      };
      const bool returned=a::RunMotionConfig({},presenter,driver,root);sample_input={};
      const bool early=scenario=="swap-config-reject" || scenario=="swap-record-throw";
      const bool motion_ok=!early;
      const unsigned captured_motion=scenario=="swap-config-reject" ? 1u : scenario=="swap-record-throw" ? 6u : 25u;
      const unsigned saved_motion=scenario=="swap-record-throw" ? 5u : captured_motion;
      const unsigned stored_sources=scenario=="swap-config-reject" ? 1u : scenario=="swap-record-throw" ? 5u : 25u;
      const bool drain=scenario!="timer-finish-fail";
      const bool timer_written=drain && scenario!="timer-write-fail";
      const bool swap_payload=swap && scenario!="swap-serialize-throw" &&
          scenario!="precedence-swap-phase" && scenario!="precedence-swap-config";
      const bool swap_written=swap_payload && scenario!="swap-write-zero" &&
          scenario!="swap-write-partial" && scenario!="swap-flush-fail";
      const bool phase_payload=scenario!="phase-finish-fail";
      const bool phase_written=phase_payload && scenario!="phase-write-fail" && scenario!="precedence-swap-phase";
      const bool config_written=scenario!="config-fail" && scenario!="precedence-swap-config";
      const bool success=motion_ok && timer_written && phase_written && config_written &&
          (!swap || (swap_written && swap_attempt.configured && stored_sources==25));
      Need(returned==success && !contract_failed && !event_overflow && !forbidden,
           "exact result and stub contracts; no actual Runtime/SDK/clock calls");
      Need(timer_begins==1 && timer_finishes==1 && phase_begins==1 && phase_finishes==1 &&
           config_freezes==1 && config_disarms==1,"all independent phase/timer/config cuts attempted once");
      Need(timer_disarms>=1 && phase_disarms>=1 && !clock_begins && !clock_finishes && !history_finishes,
           "cleanup disarms, old clock/history remain inactive");
      Need(buttons==std::vector<uint16_t>{0x10,0x10,0x200,0x1000,0x1000,0x1000,0x1000} &&
           motion_edges==unsigned(scenario!="swap-config-reject"),"same internal navigation and bounded motion edge");
      Need(EventAt("phase-begin") && EventAt("phase-begin")<EventAt("timer-begin") &&
           EventAt("timer-finish")<EventAt("phase-finish") && EventAt("phase-finish")<EventAt("config-freeze") &&
           EventAt("config-disarm")<EventAt("terminal-write"),"independent cut and terminal order");
      Need(EventAt("timer-disarm",true)>EventAt("config-freeze") &&
           EventAt("phase-disarm",true)>EventAt("config-freeze"),"unconditional timer/phase cleanup follows all cuts");
      if (drain) Need(EventAt("timer-return") && EventAt("timer-return")<EventAt("timer-write"),
                       "timer write only after successful Finish return");
      else Need(!EventAt("timer-return") && !EventAt("clock-finish") && !EventAt("history-finish"),
                "failed timer cut does not invent drain or SDK clock stop");
      const auto nav=Json::parse(Read(root/"autostart/result.json"));
      const auto arcade=Json::parse(Read(root/"arcade-navigation/result.json"));
      const auto motion=Json::parse(Read(root/"motion-calibration/result.json"));
      Need(nav.at("frames").size()==6 && arcade.at("frames").size()==10 && arcade.at("commands").size()==6 &&
           motion.at("frames").size()==captured_motion && motion.at("capture_retry_count")==0,
           "fixed source requests, preserve failed captured slot without extra capture/search");
      Need(motion.at("status")== (motion_ok ? "MOTION_CALIBRATION_COLLECTED_REVIEW_PENDING" : "FAILED"),
           "early Record/Configure failure retained as motion failure");
      const auto last_arcade=arcade.at("frames").back().at("provenance").at("acquired_sequence").get<unsigned>();
      Need(presenter.calls==last_arcade+captured_motion,"no extra presenter request in leaf or serialization");
      for (unsigned i=0;i<captured_motion;++i) {
        const auto& frame=motion.at("frames").at(i);
        Need(frame.at("slot")==i && frame.at("saved")==(i<saved_motion),"all successful/failed frame slots explicit");
        if (scenario=="swap-record-throw" && i==5) {
          Need(frame.at("provenance").is_null(),"Record exception precedes CaptureJson and leaves raw absent");
        } else {
          const auto& expected_p=publications.at(last_arcade+i);
          Need(frame.at("provenance")==CaptureObject(expected_p),
               "legacy CaptureJson shape contains unchanged source fields, no new swap keys");
          rex::ui::GuestOutputCapture check;check.image.width=1280;check.image.height=720;check.image.stride=5120;
          check.provenance=expected_p;
          Need(a::CaptureJson(check)==LegacyCaptureBytes(expected_p),"CaptureJson byte/key-order remains unchanged ON and OFF");
        }
      }
      if (swap) {
        Need(swap_begins==1 && swap_records==captured_motion && swap_serializes==1 &&
             swap_attempt.source_captures==stored_sources && swap_attempt.attempted &&
             swap_attempt.configured==(scenario!="swap-config-reject") && swap_attempt.issuance_stop_requested &&
             synthetic_issuer_stops==unsigned(swap_attempt.configured) && swap_stop_calls>=2 &&
             EventAt("swap-stop-call",true)>EventAt("config-freeze"),
             "exact one-attempt stub Configure/source/stop accounting and unconditional final cleanup");
        Need(first_motion_capture_call==last_arcade+1 && EventAt("swap-before")<EventAt("phase-begin") &&
             EventAt("timer-begin")<EventAt("swap-begin") && EventAt("swap-begin")<EventAt("timer-finish") &&
             EventAt("timer-finish")<EventAt("swap-serialize") &&
             EventAt("swap-serialize")<EventAt("phase-finish") && EventAt("swap-stop")<EventAt("phase-finish"),
             "slot0 precedes shared once-Begin; independent swap cut follows timer and precedes phase");
        for (unsigned i=0;i<25;++i) {
          Need(received_present[i]==(i<stored_sources),"all25 source slots retained, no fabricated missing slot");
          if (received_present[i]) Need(SameProvenance(received[i],publications.at(last_arcade+i)),
              "whole SAME actual local provenance copied, including invalid SDK-result/snapshot fields");
        }
      } else Need(!swap_begins && !swap_records && !swap_serializes && !swap_stop_calls &&
                  !std::filesystem::exists(root/"motion-config/swap-source.json"),
                  "runtime OFF has no new leaf lifecycle or sidecar");
      const auto timer_bytes=Read(root/"motion-config/timer-calls.json");
      const auto phase_bytes=Read(root/"motion-config/phase-census.json");
      const auto config_bytes=Read(root/"motion-config/timer-config.json");
      Need(timer_bytes==(timer_written ? timer_text : "") && phase_bytes==(phase_written ? phase_text : "") &&
           config_bytes==(config_written ? config_text : ""),"exact retained independent raw stub bytes");
      const auto swap_bytes=swap ? Read(root/"motion-config/swap-source.json",128*1024) : "";
      const auto expected_swap=scenario=="swap-write-partial" ? swap_text.substr(0,partial_bytes) :
          swap_written || scenario=="swap-flush-fail" ? swap_text : "";
      Need(swap_bytes==expected_swap,"actual zero/prefix/full-byte-flush-failure bytes retained, not repaired JSON");
      if (swap_payload) {
        const auto raw=Json::parse(swap_text);Need(raw.at("synthetic_swap_stub")==true && raw.at("records").size()==25,
            "literal STUB payload is explicitly not SDK/production schema proof");
        for (unsigned i=0;i<25;++i) Need(raw.at("records").at(i).at("source")==
            (received_present[i] ? ProvenanceJson(publications.at(last_arcade+i)) : Json(nullptr)),
            "entire literal raw source forwarding inventory, no availability-based omission");
      }
      const std::string tf=!drain ? "ARCADE_TIMER_FINISH_FAILED" : timer_written ? "" : "ARCADE_TIMER_WRITE_FAILED";
      const std::string pf=!phase_payload ? "ARCADE_PHASE_FINISH_FAILED" : phase_written ? "" : "ARCADE_PHASE_WRITE_FAILED";
      const std::string sf=!swap_payload ? "SWAP_SOURCE_SERIALIZE_FAILED" : swap_written ? "" : "SWAP_SOURCE_WRITE_FAILED";
      const std::string failure=!config_written ? "CONFIGURATION_CUT_OR_WRITE_FAILED" : !motion_ok ? "MOTION_FAILED" :
          !tf.empty() ? tf : !pf.empty() ? pf : swap ? sf : "";
      Json expected{{"schema",swap ? "mcla-arcade-swap-binding-terminal-v1" : "mcla-arcade-timer-terminal-v2"},
          {"token",token},{"mode","stock"},{"status",success ? "COLLECTED_REVIEW_PENDING" : "FAILED"},
          {"failure",failure},{"stop_requested",false},{"navigation_collected",true},{"motion_collected",motion_ok},
          {"configuration_written",config_written},{"configuration_file","timer-config.json"},{"configuration_bytes",config_bytes.size()},
          {"timer_written",timer_written},{"timer_file","timer-calls.json"},{"timer_bytes",timer_bytes.size()},
          {"timer_failure",tf},{"timer_disarmed",true},{"before_frame_timer_covered",false},
          {"phase_written",phase_written},{"phase_file","phase-census.json"},{"phase_bytes",phase_bytes.size()},
          {"phase_failure",pf},{"phase_disarmed",true},{"before_frame_phase_covered",false},
          {"controller_disarmed",true},{"injection_requested",true},{"input_injected",true},{"os_input",false},
          {"desktop_capture",false},{"scene_proof",false},{"movement_proof",false},{"physical_speed_proof",false},
          {"fps_proof",false},{"configuration_accepted",false}};
      if (swap) {
        expected.update({{"swap_required",true},{"swap_written",swap_written},{"swap_file","swap-source.json"},
            {"swap_bytes",swap_payload ? swap_text.size() : 0},{"swap_actual_bytes",swap_bytes.size()},
            {"swap_failure",sf},{"swap_configuration_attempted",true},{"swap_configured",swap_attempt.configured},
            {"swap_issuance_stop_requested",true},{"swap_source_captures",stored_sources},
            {"before_frame_swap_covered",false},{"swap_statistics_quiescent",false},
            {"source_to_callback_association_proven",false}});
      }
      const auto terminal=Json::parse(Read(root/"motion-config/result.json"));
      Need(terminal==expected,"whole exact terminal13swap fields/status/bytes/failure precedence; OFF old shape");
      if (scenario=="swap-flush-fail") Need(!swap_written && terminal.at("swap_bytes")==terminal.at("swap_actual_bytes") &&
          swap_bytes==swap_text && terminal.at("status")=="FAILED","full reported bytes are not successful flush/durability");
      Need(write_failures==unsigned(scenario=="timer-write-fail" || scenario=="phase-write-fail" ||
          scenario=="precedence-swap-phase" ||
          scenario=="swap-write-zero" || scenario=="swap-write-partial"),"only selected owned write failure");
      Need(flush_failures==unsigned(scenario=="swap-flush-fail"),"only selected flush failure");
      Need(!std::filesystem::exists(root/"motion-config/clock-reads.json") &&
           !std::filesystem::exists(root/"motion-config/clock-history.json"),"first association lane never enables clock/history output");
      std::set<std::filesystem::path> expected_files{"motion-config/result.json","motion-config/timer-config.json",
          "motion-config/timer-calls.json","motion-config/phase-census.json","autostart/result.json",
          "arcade-navigation/result.json","motion-calibration/result.json"};
      if (swap) expected_files.insert("motion-config/swap-source.json");
      for (unsigned i=0;i<6;++i) expected_files.insert(std::filesystem::path("autostart")/
          (i ? "post-"+std::to_string(i)+".bmp" : "title.bmp"));
      for (unsigned i=0;i<10;++i) expected_files.insert(std::filesystem::path("arcade-navigation")/("frame-"+std::to_string(i)+".bmp"));
      for (unsigned i=0;i<saved_motion;++i) expected_files.insert(std::filesystem::path("motion-calibration")/
          (i ? "motion-"+std::to_string(i)+".bmp" : "before.bmp"));
      std::set<std::filesystem::path> actual_files;
      for (const auto& entry:std::filesystem::recursive_directory_iterator(root)) {
        Need(!(GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT),"no output reparse point");
        if (entry.is_directory()) continue;Need(entry.is_regular_file(),"only owned regular output");
        actual_files.insert(entry.path().lexically_relative(root));
      }
      Need(actual_files==expected_files && actual_files.size()==23+unsigned(swap)+saved_motion,
          "exact49ON/48OFF or preserved early partial image inventory");
      rex::input::X_INPUT_STATE neutral{};Need(driver.GetState(0,&neutral)==rex::X_RESULT{0} &&
          !neutral.gamepad.buttons && !neutral.gamepad.right_trigger && !driver.ArmMotion(),"terminal neutral and no rearm");
      if (swap) {
        const auto before=swap_attempt;
        Need(!mcla::arcade_swap_binding::BeginSharedWindow(phase_begin,phase_end,phase_frequency,token) &&
             swap_attempt.attempted==before.attempted && swap_attempt.source_captures==before.source_captures &&
             swap_begins==1,"stub consumed attempt cannot configure/rearm again");
      }
    }
    std::cout<<"ARCADE_SWAP_BINDING_INTEGRATION scenario="<<scenario<<" checks="<<checks
             <<" failures=0 STUB_LEAF_SYNTHETIC_PIXELS_ONLY_NO_SDK_BINDING_PROOF\n";
    std::cout.flush();std::cerr.flush();exit_code=std::cout.good() && std::cerr.good() ? 0 : 2;
  } catch (const std::exception& e) {
    swap_binding_integration::sample_input={};std::cerr<<"FAIL "<<e.what()<<'\n';std::cerr.flush();
    exit_code=std::cerr.good() ? 1 : 2;
  }
  watchdog.request_stop();watchdog.join();return exit_code;
}

