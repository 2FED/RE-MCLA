// Guest-free dense Arcade-motion integration. Compile this TU exactly once with
// MCLA_AUTOSTART_PROBE=1 and MCLA_MOTION_CALIBRATION_PROBE=1, the matching private
// provenance SDK and launcher_fs implementation; no SCOUT or duplicate native TU.
// Usage: test.exe MODE ABSOLUTE_FRESH_ROOT [96|255] (default 96).
// One mode/amplitude/process, preserve all output.
// Completes actual START/menu latches, then directly calls CompleteArcadeReceipt
// as a TEST SEAM: it represents a receipt already finished elsewhere. This test
// does NOT execute RunArcade, its file I/O, real navigation, or any game/GPU/window.
// Provenance/pixels are deliberately synthetic. Timers/input latches, RunMotion,
// production receipt/BMP writes and failure cleanup are actual implementation.
#include "mcla_autostart_win.cpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string_view>

#if !defined(MCLA_AUTOSTART_PROBE) || !defined(MCLA_MOTION_CALIBRATION_PROBE) || defined(MCLA_GUEST_SOURCE_SCOUT_PROBE)
#error Requires autostart plus motion calibration, not scout.
#endif

namespace {
namespace a = mcla::autostart;
using Json = nlohmann::json;
using namespace std::chrono_literals;
constexpr const char* kToken = "19da1e9c3517474b92abe8a11a7af5f0";
#if defined(MCLA_TEST_SPARSE_CAPTURE)
constexpr bool kSparse = true;
#else
constexpr bool kSparse = false;
#endif
constexpr unsigned kFrames = kSparse ? 3 : 25;
uint64_t Offset(unsigned slot) { return kSparse ? (slot == 1 ? 250 : 6000) : slot * 250; }
unsigned checks{};
void Need(bool ok, const char* why) {
  ++checks;
  if (!ok) throw std::runtime_error(why);
}
uint64_t Now() {
  return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
std::vector<uint8_t> Read(const std::filesystem::path& path, size_t maximum) {
  const auto length = std::filesystem::file_size(path);
  Need(length <= maximum, "bounded retained artifact");
  std::ifstream stream(path, std::ios::binary);
  std::vector<uint8_t> bytes(static_cast<size_t>(length));
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
  Need(stream.good() || (stream.eof() && size_t(stream.gcount()) == bytes.size()), "complete artifact read");
  return bytes;
}
uint32_t Le(const std::vector<uint8_t>& bytes, size_t at, unsigned count) {
  uint32_t value{};
  for (unsigned i = 0; i < count; ++i) value |= uint32_t(bytes.at(at + i)) << (8 * i);
  return value;
}
void ExactKeys(const Json& object, const std::set<std::string>& expected) {
  Need(object.is_object(), "JSON object required");
  std::set<std::string> actual;
  for (const auto& [key, value] : object.items()) actual.insert(key);
  Need(actual == expected, "exact JSON key set");
}
rex::input::X_INPUT_STATE ReadInput(a::Driver& driver) {
  rex::input::X_INPUT_STATE state{};
  Need(driver.GetState(0, &state) == rex::X_RESULT{0}, "actual user-zero read");
  Need(state.gamepad.left_trigger == 0 && state.gamepad.right_trigger == 0 &&
       state.gamepad.thumb_lx == 0 && state.gamepad.thumb_ly == 0 &&
       state.gamepad.thumb_rx == 0 && state.gamepad.thumb_ry == 0,
       "no analog input during START/menu setup");
  return state;
}
void CompleteStart(a::Driver& driver) {
  Need(driver.Arm(), "real START arm");
  const auto begin = Now(); bool saw_start = false;
  while (Now() - begin < 7000) {
    const auto state = ReadInput(driver);
    Need(state.gamepad.buttons == 0 || state.gamepad.buttons == mcla::autostart_input::kStartButton,
         "only START during startup");
    saw_start |= state.gamepad.buttons == mcla::autostart_input::kStartButton;
    const auto latch = driver.Poll();
    if (latch.status == mcla::autostart_input::Status::Succeeded) {
      Need(saw_start && latch.neutral_ack_ms && state.gamepad.buttons == 0,
           "actual five-second START then actual neutral ACK"); return;
    }
    Need(latch.status != mcla::autostart_input::Status::Failed, "START not failed");
    std::this_thread::sleep_for(5ms);
  }
  throw std::runtime_error("bounded START setup timeout");
}
void CompleteMenu(a::Driver& driver) {
  for (unsigned sequence = 1; sequence <= 6; ++sequence) {
    if (sequence > 1) std::this_thread::sleep_for(2000ms);
    Need(driver.ArmMenu(sequence), "real sequential menu arm");
    const auto begin = Now(); bool saw_down = false, completed = false;
    while (Now() - begin < 1000) {
      const auto state = ReadInput(driver);
      const auto button = mcla::arcade_input::kButtons[sequence - 1];
      Need(state.gamepad.buttons == 0 || state.gamepad.buttons == button, "only exact menu button");
      saw_down |= state.gamepad.buttons == button;
      const auto menu = driver.PollMenu();
      if (menu.status == mcla::arcade_input::Status::Succeeded) {
        Need(saw_down && menu.sequence == sequence && menu.down_ack_ms && menu.neutral_ack_ms &&
             state.gamepad.buttons == 0, "each menu down and neutral acknowledged");
        Need(menu.published_ms && menu.release_deadline_ms && menu.neutral_ack_deadline_ms &&
             *menu.release_deadline_ms == *menu.published_ms + 200 &&
             *menu.neutral_ack_deadline_ms == *menu.published_ms + 5200,
             "menu publication deadlines exact");
        completed = true; break;
      }
      Need(menu.status != mcla::arcade_input::Status::Failed, "menu not failed");
      std::this_thread::sleep_for(5ms);
    }
    Need(completed, "bounded menu setup timeout");
  }
}
struct Reader {
  std::atomic<bool> pause{false}, paused{false}, failed{false}, saw_trigger{false};
  std::jthread thread;
  Reader(a::Driver& driver, uint32_t trigger) : thread([this, &driver, trigger](std::stop_token stop) {
    try {
      while (!stop.stop_requested()) {
        if (pause.load()) { paused.store(true); std::this_thread::sleep_for(2ms); continue; }
        paused.store(false);
        rex::input::X_INPUT_STATE state{};
        if (driver.GetState(0, &state) != rex::X_RESULT{0} || state.gamepad.buttons != 0 ||
            state.gamepad.left_trigger != 0 || state.gamepad.thumb_lx != 0 || state.gamepad.thumb_ly != 0 ||
            state.gamepad.thumb_rx != 0 || state.gamepad.thumb_ry != 0 ||
            (state.gamepad.right_trigger != 0 && state.gamepad.right_trigger != trigger)) {
          failed.store(true); return;
        }
        if (state.gamepad.right_trigger == trigger) saw_trigger.store(true);
        std::this_thread::sleep_for(2ms);
      }
    } catch (...) { failed.store(true); }
  }) {}
  void PauseAndWait() {
    pause.store(true); const auto begin = Now();
    while (!paused.load() && !failed.load() && Now() - begin < 500) std::this_thread::sleep_for(2ms);
    Need(paused.load() && !failed.load(), "reader paused before blocking capture");
  }
  ~Reader() { if (thread.joinable()) { thread.request_stop(); thread.join(); } }
};
class SyntheticPresenter final : public rex::ui::Presenter {
 public:
  SyntheticPresenter(std::string mode, a::Driver& driver, Reader& reader, std::stop_source& stop)
      : Presenter({}), mode_(std::move(mode)), driver_(driver), reader_(reader), stop_(stop) {}
  rex::ui::Surface::TypeFlags GetSupportedSurfaceTypes() const override { return 0; }
  bool CaptureGuestOutput(rex::ui::RawImage&) override { throw std::runtime_error("wrong capture API"); }
  unsigned calls{};
  bool release_observed_independently = false;
  rex::ui::GuestCaptureStatus CaptureGuestOutputWithProvenance(rex::ui::GuestOutputCapture& out) override {
    ++calls;
    if (mode_ == "final-timeout" && calls == kFrames) std::this_thread::sleep_for(4500ms);
    if (calls == 2) {
      if (mode_ == "capture-fail") return rex::ui::GuestCaptureStatus::kFailed;
      if (mode_ == "missing-neutral") reader_.PauseAndWait();
      if (mode_ == "late") std::this_thread::sleep_for(kSparse ? 6500ms : 1600ms);
      if (mode_ == "poll-release" || mode_ == "stop-block") {
        reader_.PauseAndWait(); const auto before = driver_.PollMotion();
        Need(before.down_ack_ms && before.published_ms &&
             before.status == mcla::motion_input::Status::HoldingRightTrigger, "real RT ACK before capture block");
        if (mode_ == "stop-block") { stop_.request_stop(); std::this_thread::sleep_for(1000ms); }
        else std::this_thread::sleep_for(3500ms);
        const auto observed_at = Now(); const auto after = driver_.PollMotion();
        Need(after.released_ms && after.released_clock_valid && *after.released_ms + 250 < observed_at,
             "independent poller released while both capture and guest reads blocked");
        Need(!after.neutral_ack_ms, "poller cannot fabricate neutral ACK");
        if (mode_ == "poll-release") {
          Need(*after.released_ms >= *before.published_ms + 3000 &&
               *after.released_ms <= *before.published_ms + 3500 &&
               after.status == mcla::motion_input::Status::WaitingNeutralAck, "bounded independent deadline release");
        } else {
          Need(after.status == mcla::motion_input::Status::Failed &&
               after.error == mcla::motion_input::Error::Cancelled, "independent cancellation during capture");
        }
        release_observed_independently = true;
        if (kSparse && mode_ == "poll-release") reader_.pause.store(false);
      }
    }
    out = {}; out.image.width = 1280; out.image.height = 720; out.image.stride = 5120;
    out.image.data.resize(size_t(1280) * 720 * 4);
    for (size_t i = 0; i < out.image.data.size(); i += 4) {
      out.image.data[i] = uint8_t(calls); out.image.data[i + 1] = 37;
      out.image.data[i + 2] = 83; out.image.data[i + 3] = 255;
    }
    // Synthetic monotonic provenance only; never an observed GPU/clock claim.
    auto& p = out.provenance; p.presenter_epoch = 1; p.acquired_sequence = calls; p.qpc_frequency = 10000000;
    p.refresh_cpu_begin_qpc = p.refresh_cpu_end_qpc = p.acquire_before_qpc = p.acquire_after_qpc =
      p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = 100 + calls;
    return rex::ui::GuestCaptureStatus::kSuccess;
  }
 protected:
  SurfacePaintConnectResult ConnectOrReconnectPaintingToSurfaceFromUIThread(
      rex::ui::Surface&, uint32_t, uint32_t, bool, bool&) override { throw std::runtime_error("no surface permitted"); }
  void DisconnectPaintingFromSurfaceFromUIThreadImpl() override {}
  bool RefreshGuestOutputImpl(uint32_t, uint32_t, uint32_t,
      std::function<bool(GuestOutputRefreshContext&)>, bool&) override { throw std::runtime_error("no refresh permitted"); }
  PaintResult PaintAndPresentImpl(bool) override { throw std::runtime_error("no presentation permitted"); }
 private:
  std::string mode_; a::Driver& driver_; Reader& reader_; std::stop_source& stop_;
};
void Verify(const std::filesystem::path& root, const std::string& mode,
            bool returned, unsigned calls, bool independent, bool saw_trigger, uint32_t trigger) {
  const auto folder = root / L"motion-calibration";
  Need(!std::filesystem::exists(root / L"arcade-navigation"), "completion seam does not invent Arcade receipt I/O");
  if (mode == "cancel-before-completion") {
    Need(!returned && calls == 0 && !saw_trigger && !std::filesystem::exists(folder),
         "cancelled completion rejects before capture/output/input"); return;
  }
  const auto bytes = Read(folder / L"result.json", 65536);
  const auto j = Json::parse(bytes.begin(), bytes.end());
  ExactKeys(j, {"schema", "token", "status", "failure", "right_trigger", "input_policy", "actor", "input_injected",
    "receipt_scope", "race_ready_proven", "os_input", "desktop_capture", "scene_proven", "movement_proven",
    "physical_speed_proven", "fps_proven", "physical_hold_duration_proven", "cleanup_proven", "capture_retry_count",
    "capture_offsets_ms", "request_lateness_budget_ms", "publication_budget_ms", "poll_interval_ms", "input_status",
    "input_error", "start_neutral_acknowledged", "published_ms", "release_deadline_ms", "neutral_ack_deadline_ms",
    "released_ms", "released_clock_valid", "first_failure_observed_ms", "first_failure_clock_valid", "down_ack_ms",
    "neutral_ack_ms", "read_count", "poll_count", "down_read_count", "neutral_read_count", "frames"});
  Need(j.at("schema") == (kSparse ? "mcla-arcade-sparse-motion-v1" : "mcla-arcade-motion-v1") &&
       j.at("token") == kToken, "exact schedule schema/token");
  Need(j.at("right_trigger").is_number_unsigned() && j.at("right_trigger") == trigger &&
       j.at("input_policy") == "exclusive_internal_rt" + std::to_string(trigger) + "_3000ms_after_arcade_neutral" &&
       j.at("actor") == "native_diagnostic_driver" && j.at("input_injected") == true &&
       j.at("receipt_scope") == "motion_only_after_arcade", "exact Arcade policy/scope");
  // input_injected is the production policy label, NOT evidence that a failed
  // arm injected input. Missing-completion separately checks zero RT below.
  for (const auto* key : {"race_ready_proven", "os_input", "desktop_capture", "scene_proven", "movement_proven",
       "physical_speed_proven", "fps_proven", "physical_hold_duration_proven", "cleanup_proven"})
    Need(j.at(key).is_boolean() && !j.at(key).get<bool>(), "all semantic proof flags stay false");
  Need(j.at("capture_retry_count") == 0 && j.at("request_lateness_budget_ms") == 250 &&
       j.at("publication_budget_ms") == 10000 && j.at("poll_interval_ms") == 10, "exact bounds/no retries");
  Need(j.at("capture_offsets_ms").is_array() && j.at("capture_offsets_ms").size() == kFrames - 1,
       "exact schedule offset count");
  for (unsigned i = 0; i < kFrames - 1; ++i)
    Need(j.at("capture_offsets_ms")[i].is_number_unsigned() && j.at("capture_offsets_ms")[i] == Offset(i + 1),
         "exact predeclared schedule");
  const bool success = mode == "success" || (kSparse && mode == "poll-release");
  const bool missing = mode == "missing-arcade-completion";
  Need(returned == success && j.at("status") == (success ? "MOTION_CALIBRATION_COLLECTED_REVIEW_PENDING" : "FAILED"),
       "terminal outcome");
  uint64_t publication{};
  if (missing) {
    Need(calls == 1 && !saw_trigger && j.at("failure") == "MOTION_ARM_FAILED" && j.at("published_ms").is_null() &&
         j.at("down_ack_ms").is_null() && j.at("neutral_ack_ms").is_null(), "missing completion never publishes RT");
  } else {
    publication = j.at("published_ms").get<uint64_t>();
    Need(saw_trigger && j.at("release_deadline_ms") == publication + 3000 &&
         j.at("neutral_ack_deadline_ms") == publication + 8000 && j.at("start_neutral_acknowledged") == true,
         "actual publication anchored motion deadlines");
  }
  if (success) {
    Need(calls == kFrames && j.at("frames").size() == kFrames && j.at("failure") == "" &&
         !j.at("down_ack_ms").is_null() && !j.at("neutral_ack_ms").is_null(), "exact captures and actual input ACKs");
  } else if (mode == "capture-fail") Need(calls == 2 && j.at("failure") == "MOTION_CAPTURE_FAILED", "deliberate capture failure");
  else if (mode == "final-timeout")
    Need(calls == kFrames && j.at("failure") == "MOTION_CAPTURE_BUDGET", "late final completion rejected without pixels");
  else if (mode == "missing-neutral")
    Need(calls == kFrames && j.at("failure") == "MOTION_INPUT_FAILED" && j.at("neutral_ack_ms").is_null(),
         "missing neutral ACK is not fabricated by poller or images");
  else if (mode == "late" || mode == "poll-release")
    Need(calls == 2 && j.at("failure") == "MOTION_SLOT_LATE", "late next slot rejected without catch-up");
  else if (mode == "stop-block") Need(calls == 2 && j.at("failure") == "MOTION_STOP_OR_POLLER_FAILED", "blocked cancellation retained");
  if (mode == "poll-release" || mode == "stop-block") Need(independent, "independent poll/release assertion executed");
  const unsigned expected_records = success || mode == "final-timeout" || mode == "missing-neutral" ?
      kFrames : (missing ? 1 : (mode == "late" || mode == "poll-release" ? 3 : 2));
  Need(j.at("frames").size() == expected_records, "exact failed/complete observation prefix");
  std::set<std::wstring> expected{L"result.json"}; unsigned saved{}, index{};
  for (const auto& frame : j.at("frames")) {
    ExactKeys(frame, {"slot", "due_ms", "request_before_ms", "request_after_ms", "capture_status", "saved", "provenance"});
    const auto slot = frame.at("slot").get<unsigned>(); Need(slot == index++, "contiguous recorded slots");
    Need(slot ? frame.at("due_ms") == publication + Offset(slot) : frame.at("due_ms").is_null(), "exact publication-relative slot");
    if (!frame.at("saved").get<bool>()) continue;
    const auto before = frame.at("request_before_ms").get<uint64_t>();
    const auto after = frame.at("request_after_ms").get<uint64_t>();
    Need(after >= before && frame.at("capture_status") == static_cast<int>(rex::ui::GuestCaptureStatus::kSuccess),
         "saved capture status and monotonic request bracket");
    if (slot) Need(before >= publication + Offset(slot) && before <= publication + Offset(slot) + 250,
                   "saved capture request inside exact lateness budget");
    const std::wstring name = slot ? L"motion-" + std::to_wstring(slot) + L".bmp" : L"before.bmp";
    expected.insert(name); ++saved; const auto bmp = Read(folder / name, 4 * 1024 * 1024);
    Need(bmp.size() == 54 + size_t(1280) * 720 * 3 && bmp[0] == 'B' && bmp[1] == 'M' &&
         Le(bmp, 18, 4) == 1280 && Le(bmp, 22, 4) == 720 && Le(bmp, 28, 2) == 24, "actual BMP header and size");
    // Every retained pixel, not just one sample, must match this capture's RGB.
    for (size_t at = 54; at < bmp.size(); at += 3)
      if (bmp[at] != 83 || bmp[at + 1] != 37 || bmp[at + 2] != uint8_t(slot + 1))
        throw std::runtime_error("retained synthetic pixel mismatch");
    Need(true, "all retained BMP pixels match synthetic capture identity");
    const auto& provenance = frame.at("provenance");
    ExactKeys(provenance, {"width", "height", "stride", "presenter_epoch", "acquired_sequence", "qpc_frequency",
      "refresh_cpu_begin_qpc", "refresh_cpu_end_qpc", "acquire_before_qpc", "acquire_after_qpc",
      "copy_submit_before_qpc", "copy_completion_observed_qpc", "pixels_ready_qpc"});
    Need(provenance.at("acquired_sequence") == slot + 1 && provenance.at("presenter_epoch") == 1 &&
         provenance.at("width") == 1280 && provenance.at("height") == 720 && provenance.at("stride") == 5120 &&
         provenance.at("qpc_frequency") == 10000000, "synthetic captured identity/layout retained");
    for (const auto* key : {"refresh_cpu_begin_qpc", "refresh_cpu_end_qpc", "acquire_before_qpc", "acquire_after_qpc",
         "copy_submit_before_qpc", "copy_completion_observed_qpc", "pixels_ready_qpc"})
      Need(provenance.at(key) == 101 + slot, "exact synthetic provenance retained without timing claims");
  }
  const unsigned expected_saved = success || mode == "missing-neutral" ? kFrames :
      mode == "final-timeout" ? kFrames - 1 : (mode == "late" || mode == "poll-release" ? 2u : 1u);
  Need(saved == expected_saved, "exact saved count; no rejected pixels invented");
  std::set<std::wstring> actual;
  for (const auto& item : std::filesystem::directory_iterator(folder)) {
    Need(item.is_regular_file(), "only owned files"); actual.insert(item.path().filename().wstring());
  }
  Need(actual == expected, "exact retained artifact set");
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
  // Parent also enforces an external timeout. This only terminates its own
  // guest-free test process, preserving partial files; it never owns a game.
  std::jthread watchdog([](std::stop_token stop) {
    const auto begin = Now();
    while (!stop.stop_requested() && Now() - begin < 45000) std::this_thread::sleep_for(20ms);
    if (!stop.stop_requested()) std::_Exit(4);
  });
  int code = 1;
  try {
    Need(argc == 3 || argc == 4, "MODE ABSOLUTE_FRESH_ROOT [96|255] required");
    Need(argc == 3 || std::wstring_view(argv[3]) == L"96" || std::wstring_view(argv[3]) == L"255",
         "optional amplitude must be exactly 96 or 255");
    const uint32_t trigger = argc == 4 && std::wstring_view(argv[3]) == L"255" ? 255 : 96;
    Need(!kSparse || trigger == 255, "sparse test requires exact RT255");
    const std::wstring wide = argv[1]; const std::string mode(wide.begin(), wide.end());
    Need(std::set<std::string>{"success", "capture-fail", "late", "poll-release", "stop-block",
         "missing-arcade-completion", "cancel-before-completion", "final-timeout", "missing-neutral"}.contains(mode), "known mode");
    const std::filesystem::path root = argv[2];
    Need(root.is_absolute() && root == root.lexically_normal() && !root.filename().empty() &&
         !std::filesystem::exists(root), "fresh absolute canonical root");
    auto parent = mcla::launcher::fs::OpenDirectory(root.parent_path().wstring());
    Need(parent.has_value(), "existing safe parent only");
    Need(std::filesystem::create_directory(root), "exclusive fresh owned root");
    Need(!a::Enabled() && !a::MotionEnabled() && !REXCVAR_GET(mcla_arcade_navigation_probe), "compiled defaults OFF");
    Need(REXCVAR_GET(mcla_motion_calibration_trigger) == 96, "compiled default RT96");
    Need(!REXCVAR_GET(mcla_arcade_sparse_capture), "sparse capture defaults OFF");
    REXCVAR_SET(mcla_autostart_probe, true); REXCVAR_SET(mcla_autostart_token, kToken);
    REXCVAR_SET(mcla_arcade_navigation_probe, true); REXCVAR_SET(mcla_motion_calibration_probe, true);
    REXCVAR_SET(mcla_motion_calibration_trigger, trigger);
    REXCVAR_SET(mcla_arcade_sparse_capture, kSparse);
    a::ValidateConfig();
    a::Driver driver; Need(driver.Setup() == rex::X_STATUS{0}, "actual driver setup");
    CompleteStart(driver); CompleteMenu(driver);
    std::stop_source stop;
    if (mode == "cancel-before-completion") {
      stop.request_stop();
      Need(!driver.CompleteArcadeReceipt(stop.get_token(), 10, 6), "cancelled completion seam rejected");
    } else if (mode != "missing-arcade-completion") {
      Need(driver.CompleteArcadeReceipt(stop.get_token(), 10, 6), "explicit already-finished Arcade receipt seam");
    }
    Reader reader(driver, trigger); SyntheticPresenter presenter(mode, driver, reader, stop);
    const bool result = a::RunMotion(stop.get_token(), presenter, driver, root);
    reader.thread.request_stop(); reader.thread.join();
    Need(!reader.failed.load(), "motion reads only exact selected RT channel");
    const auto neutral = ReadInput(driver);
    Need(neutral.gamepad.buttons == 0, "terminal response fully neutral after success/failure");
    Verify(root, mode, result, presenter.calls, presenter.release_observed_independently, reader.saw_trigger.load(), trigger);
    std::cout << "Arcade " << (kSparse ? "sparse" : "dense") << " motion capture " << mode << " RT" << trigger << ": " << checks
              << " checks, 0 failures; SYNTHETIC_ONLY, COMPLETION_SEAM_NOT_REAL_ARCADE_IO\n" << std::flush;
    code = 0;
  } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n' << std::flush; }
  watchdog.request_stop(); watchdog.join(); return code;
}
