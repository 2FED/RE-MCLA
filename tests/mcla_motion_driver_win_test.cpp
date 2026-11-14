// Guest-free integration test of the ACTUAL Driver and ValidateConfig.
// Include this implementation once; never also compile autostart_win.cpp as a TU.
// Requires the matching provenance SDK headers/runtime, launcher_fs_win.cpp and
// the same SDK/UI/system libraries as scout-autostart-command.json. No stubs.
// Compile variants: AUTOSTART only; AUTOSTART+MOTION; AUTOSTART+MOTION+SCOUT.
// For SCOUT, additionally compile actual guest_source_scout_win.cpp (no other
// probe macros). Never call Run/RunMotion, Presenter, Setup of a runtime, or game.
// Real steady-clock waits: START currently 5000ms, RT 3000ms. An external owned
// process timeout of 60s is recommended. No filesystem writes or OS input.
// The synthetic blocked caller tests Driver concurrency, not RunMotion wiring.
#include "mcla_autostart_win.cpp" // Build include path selects the frozen source copy.
#include <iostream>
#include <string_view>

#if defined(MCLA_GUEST_SOURCE_SCOUT_PROBE)
REXCVAR_DECLARE(bool, mcla_source_scout_probe);
#else
REXCVAR_DEFINE_BOOL(mcla_source_scout_probe, false, "Test", "Registry-only competing flag");
#endif
REXCVAR_DEFINE_BOOL(mcla_raw_config_experiment, false, "Test", "Registry-only competing flag");
REXCVAR_DEFINE_BOOL(mcla_fixed60_config_experiment, false, "Test", "Registry-only competing flag");
REXCVAR_DEFINE_BOOL(mcla_driver_unknown_probe, false, "Test", "Unknown competing probe");
REXCVAR_DEFINE_BOOL(mcla_driver_unknown_experiment, false, "Test", "Unknown competing experiment");

namespace {
namespace probe = mcla::autostart;
using Driver = probe::Driver;
using StartStatus = mcla::autostart_input::Status;
using InputState = rex::input::X_INPUT_STATE;
using TestClock = std::chrono::steady_clock;
using namespace std::chrono_literals;
unsigned checks = 0, failures = 0;

void Check(bool ok, const char* label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Need(bool ok, const char* label) {
  Check(ok, label);
  if (!ok) throw std::runtime_error(label);
}
template<class F> void Throws(F&& f, const char* reason) {
  bool expected = false;
  try { f(); }
  catch (const std::runtime_error& e) { expected = std::string_view(e.what()) == reason; }
  catch (...) {}
  Check(expected, reason);
}
void AcceptsConfig(const char* label) {
  bool accepted = false;
  try { probe::ValidateConfig(); accepted = true; }
  catch (const std::exception& e) { std::cerr << label << ": " << e.what() << '\n'; }
  Check(accepted, label);
}

// Fill every field so stale caller contents cannot masquerade as zeroing.
InputState Read(rex::input::InputDriver& driver, uint16_t buttons, uint8_t rt) {
  InputState out{};
  out.packet_number = 0xFFFFFFFFu; out.gamepad.buttons = 0xFFFF;
  out.gamepad.left_trigger = 255; out.gamepad.right_trigger = 255;
  out.gamepad.thumb_lx = 100; out.gamepad.thumb_ly = -101;
  out.gamepad.thumb_rx = 102; out.gamepad.thumb_ry = -103;
  Check(driver.GetState(0, &out) == rex::X_RESULT{0}, "GetState succeeds");
  Check(uint16_t(out.gamepad.buttons) == buttons, "exact button word");
  Check(out.gamepad.right_trigger == rt, "exact RT byte");
  Check(out.gamepad.left_trigger == 0 && int16_t(out.gamepad.thumb_lx) == 0 &&
        int16_t(out.gamepad.thumb_ly) == 0 && int16_t(out.gamepad.thumb_rx) == 0 &&
        int16_t(out.gamepad.thumb_ry) == 0, "all unrelated analog channels zero");
  return out;
}
void RegistryCases() {
  Check(!probe::Enabled(), "autostart defaults off");
  Check(REXCVAR_GET(mcla_autostart_start_delay_ms) == 0, "START delay defaults zero");
  REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
  Throws([] { probe::ValidateConfig(); }, "AUTOSTART_DELAY_REQUIRES_FIXED60_MOTION_CONFIG");
  REXCVAR_SET(mcla_autostart_start_delay_ms, 1u);
  Throws([] { probe::ValidateConfig(); }, "AUTOSTART_INVALID_START_DELAY");
  REXCVAR_SET(mcla_autostart_start_delay_ms, 0u);
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
  Check(!probe::MotionEnabled(), "motion defaults off");
  Check(REXCVAR_GET(mcla_motion_calibration_trigger) == 96, "default motion amplitude is 96");
#else
  Check(rex::cvar::GetFlagInfo("mcla_motion_calibration_probe") == nullptr,
        "motion flag absent in non-motion build");
  Check(rex::cvar::GetFlagInfo("mcla_motion_calibration_trigger") == nullptr,
        "amplitude flag absent in non-motion build");
#endif
  AcceptsConfig("default-off admission has no side effects");
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  Throws([] { probe::ValidateConfig(); }, "MOTION_TRIGGER_REQUIRES_MOTION");
  for (uint32_t invalid : {0u, 1u, 95u, 97u, 254u, 256u, 352u, UINT32_MAX}) {
    REXCVAR_SET(mcla_motion_calibration_trigger, invalid);
    Throws([] { probe::ValidateConfig(); }, "MOTION_INVALID_RIGHT_TRIGGER");
  }
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  Throws([] { probe::ValidateConfig(); }, "MOTION_REQUIRES_AUTOSTART");
  REXCVAR_SET(mcla_motion_calibration_probe, false);
#endif
  REXCVAR_SET(mcla_autostart_probe, true);
  Throws([] { probe::ValidateConfig(); }, "AUTOSTART_INVALID_TOKEN");
  REXCVAR_SET(mcla_autostart_token, "0123456789abcdef0123456789abcdef");
  AcceptsConfig("START-only configuration retained");
  REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
  Throws([] { probe::ValidateConfig(); }, "AUTOSTART_DELAY_REQUIRES_FIXED60_MOTION_CONFIG");
  REXCVAR_SET(mcla_autostart_start_delay_ms, 0u);
  REXCVAR_SET(mcla_driver_unknown_probe, true);
  Throws([] { probe::ValidateConfig(); }, "AUTOSTART_OTHER_PROBE_ENABLED");
  REXCVAR_SET(mcla_driver_unknown_probe, false);
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  AcceptsConfig("isolated motion configuration accepted");
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  AcceptsConfig("isolated full amplitude configuration accepted");
  REXCVAR_SET(mcla_motion_calibration_trigger, 352u);
  Throws([] { probe::ValidateConfig(); }, "MOTION_INVALID_RIGHT_TRIGGER");
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
  REXCVAR_SET(mcla_source_scout_probe, true);
  Throws([] { probe::ValidateConfig(); }, "MOTION_OTHER_PROBE_OR_EXPERIMENT_ENABLED");
  REXCVAR_SET(mcla_source_scout_probe, false);
  REXCVAR_SET(mcla_raw_config_experiment, true);
  Throws([] { probe::ValidateConfig(); }, "MOTION_OTHER_PROBE_OR_EXPERIMENT_ENABLED");
  REXCVAR_SET(mcla_raw_config_experiment, false);
  REXCVAR_SET(mcla_fixed60_config_experiment, true);
  Throws([] { probe::ValidateConfig(); }, "MOTION_OTHER_PROBE_OR_EXPERIMENT_ENABLED");
  REXCVAR_SET(mcla_fixed60_config_experiment, false);
  REXCVAR_SET(mcla_driver_unknown_probe, true);
  Throws([] { probe::ValidateConfig(); }, "MOTION_OTHER_PROBE_OR_EXPERIMENT_ENABLED");
  REXCVAR_SET(mcla_driver_unknown_probe, false);
  REXCVAR_SET(mcla_driver_unknown_experiment, true);
  Throws([] { probe::ValidateConfig(); }, "MOTION_OTHER_PROBE_OR_EXPERIMENT_ENABLED");
  REXCVAR_SET(mcla_driver_unknown_experiment, false);
  AcceptsConfig("motion admission recovers only after competing flags disabled");
  REXCVAR_SET(mcla_motion_calibration_probe, false);
#endif
}
void PublicDriverCases() {
  auto driver = probe::CreateDriver();
  Check(driver->Setup() == rex::X_STATUS{0}, "actual input driver Setup only");
  const auto initial = Read(*driver, 0, 0);
  Check(uint32_t(initial.packet_number) == 1, "initial packet");
  Check(uint32_t(Read(*driver, 0, 0).packet_number) == 1, "unchanged packet stable");
  InputState sentinel{}; sentinel.packet_number = 123;
  Check(driver->GetState(1, &sentinel) == rex::X_RESULT{0x48F} &&
        uint32_t(sentinel.packet_number) == 123, "invalid user leaves output untouched");
  Check(driver->GetState(0, nullptr) == rex::X_RESULT{0x48F}, "null state rejected");
  rex::input::X_INPUT_CAPABILITIES caps{};
  Check(driver->GetCapabilities(0, 0, &caps) == rex::X_RESULT{0} &&
        uint16_t(caps.gamepad.buttons) == 0x10 && caps.gamepad.right_trigger == 0,
        "motion-off capabilities preserve START-only mask");
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  Check(driver->GetCapabilities(0, 0, &caps) == rex::X_RESULT{0} &&
        caps.gamepad.right_trigger == 255 && caps.gamepad.left_trigger == 0 &&
        int16_t(caps.gamepad.thumb_lx) == 0 && int16_t(caps.gamepad.thumb_ly) == 0 &&
        int16_t(caps.gamepad.thumb_rx) == 0 && int16_t(caps.gamepad.thumb_ry) == 0,
        "motion capabilities RT mask only, not actual RT value");
  Read(*driver, 0, 0); // Enabling the flag does not arm or inject anything.
#endif
}

// No fake clock or manual latch substitution. Poll waits for the real START
// deadline and deliberately does not grant the guest's neutral acknowledgment.
void CompleteStart(Driver& driver) {
  Need(driver.Arm(), "START arms once");
  Check(uint32_t(Read(driver, 0x10, 0).packet_number) == 2, "START packet transition");
  Check(uint32_t(Read(driver, 0x10, 0).packet_number) == 2, "held START packet stable");
  const auto limit = TestClock::now() + 7s;
  auto state = driver.Poll();
  while (state.status == StartStatus::HoldingStart && TestClock::now() < limit) {
    std::this_thread::sleep_for(5ms); state = driver.Poll();
  }
  Need(state.status == StartStatus::WaitingNeutralAck && !state.neutral_ack_ms,
       "poll releases START without inventing neutral ACK");
  Check(uint32_t(Read(driver, 0, 0).packet_number) == 3, "START neutral packet transition");
  state = driver.Poll();
  Need(state.status == StartStatus::Succeeded && state.down_ack_ms && state.neutral_ack_ms,
       "actual GetState supplies START neutral prerequisite");
}

#if defined(MCLA_MOTION_CALIBRATION_PROBE)
using MotionStatus = mcla::motion_input::Status;
// A bounded synthetic capture caller holds no Driver lock. This is intentionally
// NOT a fake Presenter and does not exercise RunMotion or receipt/file output.
class BlockedCaller {
 public:
  BlockedCaller() : thread_([this](std::stop_token stop) {
    std::unique_lock lock(mutex_); entered_ = true; changed_.notify_all();
    changed_.wait_for(lock, stop, 12s, [&] { return release_; });
    finished_.store(true);
  }) {
    std::unique_lock lock(mutex_);
    Need(changed_.wait_for(lock, 1s, [&] { return entered_; }), "synthetic capture caller entered");
  }
  ~BlockedCaller() { thread_.request_stop(); changed_.notify_all(); }
  bool finished() const { return finished_.load(); }
 private:
  std::mutex mutex_; std::condition_variable_any changed_;
  bool entered_ = false, release_ = false;
  std::atomic<bool> finished_{false};
  std::jthread thread_; // Destroy/join before synchronization members.
};
void PrerequisiteCases() {
  Driver idle;
  Check(!idle.ArmMotion(), "motion cannot arm before START");
  Check(idle.PollMotion().error == mcla::motion_input::Error::StartNeutralNotAcknowledged,
        "missing START prerequisite terminal error");
  Read(idle, 0, 0);
  Driver down;
  Need(down.Arm(), "prerequisite test START arm");
  Read(down, 0x10, 0);
  Check(!down.ArmMotion(), "down ACK alone is not START neutral ACK");
  Check(down.PollMotion().status == MotionStatus::Failed, "premature motion fails closed");
  down.Fail(); Read(down, 0, 0);
}
void MotionSequenceAndIndependentRelease(uint32_t amplitude) {
  REXCVAR_SET(mcla_motion_calibration_trigger, amplitude);
  Driver driver; CompleteStart(driver);
  Need(driver.ArmMotion(), "motion arm after actual START neutral ACK");
  Check(driver.PollMotion().right_trigger == amplitude, "driver latched selected amplitude");
  // Direct registry edits are test-only (production flag is init-only). Once
  // armed, guest responses must still come from the immutable latch value.
  REXCVAR_SET(mcla_motion_calibration_trigger, amplitude == 96 ? 255u : 96u);
  Check(uint32_t(Read(driver, 0, uint8_t(amplitude)).packet_number) == 4, "RT packet transition");
  Check(uint32_t(Read(driver, 0, uint8_t(amplitude)).packet_number) == 4, "held RT packet stable despite registry change");
  REXCVAR_SET(mcla_motion_calibration_trigger, amplitude);
  const auto armed = driver.PollMotion();
  Need(armed.published_ms && armed.down_ack_ms && armed.release_deadline_ms &&
       armed.neutral_ack_deadline_ms, "motion publication and down ACK recorded");
  Check(*armed.release_deadline_ms == *armed.published_ms + 3000 &&
        *armed.neutral_ack_deadline_ms == *armed.published_ms + 8000,
        "driver retains exact publication-anchored deadlines");
  BlockedCaller capture;
  std::atomic<bool> released{false};
  std::jthread poller([&](std::stop_token stop) {
    const auto limit = TestClock::now() + 5s;
    while (!stop.stop_requested() && TestClock::now() < limit) {
      const auto state = driver.PollMotion();
      if (state.status != MotionStatus::HoldingRightTrigger) {
        released.store(state.status == MotionStatus::WaitingNeutralAck); return;
      }
      std::this_thread::sleep_for(5ms);
    }
  });
  const auto limit = TestClock::now() + 5s;
  while (!released.load() && TestClock::now() < limit) std::this_thread::sleep_for(5ms);
  Check(released.load() && !capture.finished(), "independent poll releases while capture caller blocked");
  const auto released_state = driver.PollMotion();
  Check(released_state.released_ms && released_state.released_clock_valid &&
        *released_state.released_ms >= *armed.release_deadline_ms && !released_state.neutral_ack_ms,
        "actual release retained; poll never manufactures neutral ACK");
  Check(uint32_t(Read(driver, 0, 0).packet_number) == 5, "RT neutral packet transition");
  Check(driver.PollMotion().status == MotionStatus::Succeeded, "guest neutral ACK completes motion");
  Check(uint32_t(Read(driver, 0, 0).packet_number) == 5, "terminal neutral packet stable");
  Check(!driver.ArmMotion(), "actual driver cannot repeat motion arm");
  Read(driver, 0, 0);
}
void IndependentCancellation() {
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
  Driver driver; CompleteStart(driver);
  Need(driver.ArmMotion(), "cancellation test motion arms");
  Read(driver, 0, 96);
  BlockedCaller capture;
  std::stop_source cancellation;
  std::atomic<bool> cancelled{false};
  std::jthread poller([&](std::stop_token own_stop) {
    const auto limit = TestClock::now() + 2s;
    while (!own_stop.stop_requested() && TestClock::now() < limit) {
      if (cancellation.stop_requested()) { driver.CancelMotion(); cancelled.store(true); return; }
      driver.PollMotion(); std::this_thread::sleep_for(5ms);
    }
  });
  cancellation.request_stop();
  const auto limit = TestClock::now() + 2s;
  while (!cancelled.load() && TestClock::now() < limit) std::this_thread::sleep_for(5ms);
  Check(cancelled.load() && !capture.finished(), "independent cancellation while capture caller blocked");
  const auto state = driver.PollMotion();
  Check(state.status == MotionStatus::Failed && state.error == mcla::motion_input::Error::Cancelled &&
        state.released_ms && state.released_clock_valid, "driver cancellation is terminal and timestamped");
  Check(uint32_t(Read(driver, 0, 0).packet_number) == 5, "cancelled RT returns neutral packet");
  driver.FailMotion();
  Check(driver.PollMotion().error == mcla::motion_input::Error::Cancelled, "first cancellation error retained");
  Read(driver, 0, 0);
}
void InvalidDriverAmplitude() {
  Driver driver; CompleteStart(driver);
  REXCVAR_SET(mcla_motion_calibration_trigger, 352u);
  Check(!driver.ArmMotion(), "driver rejects invalid amplitude without truncating to 96");
  Check(driver.PollMotion().error == mcla::motion_input::Error::InvalidRightTrigger &&
        !driver.PollMotion().published_ms, "invalid driver amplitude publishes nothing");
  Read(driver, 0, 0);
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
  Check(!driver.ArmMotion(), "invalid driver amplitude failure remains terminal");
  Read(driver, 0, 0);
}
#endif
} // namespace

int main() {
  try {
    RegistryCases(); PublicDriverCases();
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
    PrerequisiteCases(); MotionSequenceAndIndependentRelease(96);
    MotionSequenceAndIndependentRelease(255); IndependentCancellation(); InvalidDriverAmplitude();
#else
    Driver driver; CompleteStart(driver); Read(driver, 0, 0);
#endif
  } catch (const std::exception& e) {
    ++failures; std::cerr << "FATAL " << e.what() << '\n';
  } catch (...) { ++failures; std::cerr << "FATAL unknown exception\n"; }
  std::cout << "motion driver: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
