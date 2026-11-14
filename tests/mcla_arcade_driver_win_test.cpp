// Guest-free test of the actual private Driver and ValidateConfig, not a mock.
// Include this implementation once; do not separately compile autostart_win.cpp.
// Build AUTOSTART alone and AUTOSTART+MOTION_CALIBRATION. Link the matching real
// SDK/provenance and launcher_fs implementation; no runtime, Presenter or game.
// Real monotonic waits exercise START and six spaced menu commands. Bound this
// Usage: test.exe [96|255], default 96. Bound the owned process externally
// (150s with motion). No filesystem writes or OS input.
#include "mcla_autostart_win.cpp"
#include <iostream>
#include <string_view>

REXCVAR_DEFINE_BOOL(mcla_environment_effects_probe, false, "Test", "Competing legacy menu probe");
REXCVAR_DEFINE_BOOL(mcla_source_scout_probe, false, "Test", "Competing scout probe");
REXCVAR_DEFINE_BOOL(mcla_raw_config_experiment, false, "Test", "Competing raw experiment");
REXCVAR_DEFINE_BOOL(mcla_fixed60_config_experiment, false, "Test", "Competing fixed experiment");
REXCVAR_DEFINE_BOOL(mcla_arcade_unknown_probe, false, "Test", "Unknown competing probe");
REXCVAR_DEFINE_BOOL(mcla_arcade_unknown_experiment, false, "Test", "Unknown competing experiment");

namespace {
namespace probe = mcla::autostart;
using Driver = probe::Driver;
using StartStatus = mcla::autostart_input::Status;
using MenuStatus = mcla::arcade_input::Status;
using MenuError = mcla::arcade_input::Error;
using State = rex::input::X_INPUT_STATE;
using TestClock = std::chrono::steady_clock;
using namespace std::chrono_literals;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Need(bool value, const char* label) {
  Check(value, label);
  if (!value) throw std::runtime_error(label);
}
template<class F> void Throws(F&& function, const char* reason) {
  bool expected = false;
  try { function(); }
  catch (const std::runtime_error& error) { expected = std::string_view(error.what()) == reason; }
  catch (...) {}
  Check(expected, reason);
}
void Accepts(const char* label) {
  bool accepted = false;
  try { probe::ValidateConfig(); accepted = true; }
  catch (const std::exception& error) { std::cerr << label << ": " << error.what() << '\n'; }
  Check(accepted, label);
}
State Read(rex::input::InputDriver& driver, uint16_t buttons, uint8_t trigger = 0) {
  State state{};
  state.packet_number = 0xFFFFFFFFu; state.gamepad.buttons = 0xFFFF;
  state.gamepad.left_trigger = 255; state.gamepad.right_trigger = 255;
  state.gamepad.thumb_lx = 100; state.gamepad.thumb_ly = -101;
  state.gamepad.thumb_rx = 102; state.gamepad.thumb_ry = -103;
  Check(driver.GetState(0, &state) == rex::X_RESULT{0}, "actual GetState succeeds");
  Check(uint16_t(state.gamepad.buttons) == buttons, "exact button mask");
  Check(state.gamepad.left_trigger == 0 && state.gamepad.right_trigger == trigger &&
      int16_t(state.gamepad.thumb_lx) == 0 && int16_t(state.gamepad.thumb_ly) == 0 &&
      int16_t(state.gamepad.thumb_rx) == 0 && int16_t(state.gamepad.thumb_ry) == 0,
      "only requested RT amplitude; all other analog channels zeroed");
  return state;
}
void Registry() {
  Check(!probe::Enabled() && !REXCVAR_GET(mcla_arcade_navigation_probe), "both flags default off");
  Accepts("default configuration accepted");
  REXCVAR_SET(mcla_arcade_navigation_probe, true);
  Throws([] { probe::ValidateConfig(); }, "ARCADE_REQUIRES_STANDALONE_AUTOSTART");
  REXCVAR_SET(mcla_autostart_probe, true);
  Throws([] { probe::ValidateConfig(); }, "AUTOSTART_INVALID_TOKEN");
  REXCVAR_SET(mcla_autostart_token, "0123456789abcdef0123456789abcdef");
  Accepts("standalone Arcade accepted");
  for (uint32_t delay : {1u, 8000u}) {
    REXCVAR_SET(mcla_autostart_start_delay_ms, delay);
    Throws([] { probe::ValidateConfig(); }, "ARCADE_REQUIRES_STANDALONE_AUTOSTART");
  }
  REXCVAR_SET(mcla_autostart_start_delay_ms, 0u);
#define REJECT_COMPETING(flag) \
  REXCVAR_SET(flag, true); \
  Throws([] { probe::ValidateConfig(); }, "ARCADE_OTHER_PROBE_OR_EXPERIMENT_ENABLED"); \
  REXCVAR_SET(flag, false)
  REJECT_COMPETING(mcla_environment_effects_probe);
  REJECT_COMPETING(mcla_source_scout_probe);
  REJECT_COMPETING(mcla_raw_config_experiment);
  REJECT_COMPETING(mcla_fixed60_config_experiment);
  REJECT_COMPETING(mcla_arcade_unknown_probe);
  REJECT_COMPETING(mcla_arcade_unknown_experiment);
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  Accepts("exact Arcade plus RT96 motion admitted");
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  Accepts("exact Arcade plus RT255 motion admitted");
  Check(!REXCVAR_GET(mcla_arcade_sparse_capture), "sparse observation defaults off");
  REXCVAR_SET(mcla_arcade_sparse_capture, true);
  Accepts("exact standalone Arcade RT255 sparse observation admitted");
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
  Throws([] { probe::ValidateConfig(); }, "SPARSE_CAPTURE_REQUIRES_ARCADE_RT255");
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  REXCVAR_SET(mcla_arcade_navigation_probe, false);
  Throws([] { probe::ValidateConfig(); }, "SPARSE_CAPTURE_REQUIRES_ARCADE_RT255");
  REXCVAR_SET(mcla_motion_calibration_probe, false);
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
  Throws([] { probe::ValidateConfig(); }, "SPARSE_CAPTURE_REQUIRES_ARCADE_RT255");
  REXCVAR_SET(mcla_autostart_probe, false);
  Throws([] { probe::ValidateConfig(); }, "SPARSE_CAPTURE_REQUIRES_ARCADE_RT255");
  REXCVAR_SET(mcla_autostart_probe, true);
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_arcade_navigation_probe, true);
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  REJECT_COMPETING(mcla_raw_config_experiment);
  REJECT_COMPETING(mcla_fixed60_config_experiment);
  REXCVAR_SET(mcla_arcade_sparse_capture, false);
  for (uint32_t invalid : {0u, 95u, 97u, 254u, 256u, UINT32_MAX}) {
    REXCVAR_SET(mcla_motion_calibration_trigger, invalid);
    Throws([] { probe::ValidateConfig(); }, "MOTION_INVALID_RIGHT_TRIGGER");
  }
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
  REJECT_COMPETING(mcla_environment_effects_probe);
  REJECT_COMPETING(mcla_source_scout_probe);
  REJECT_COMPETING(mcla_raw_config_experiment);
  REJECT_COMPETING(mcla_fixed60_config_experiment);
  REJECT_COMPETING(mcla_arcade_unknown_probe);
  REJECT_COMPETING(mcla_arcade_unknown_experiment);
  REXCVAR_SET(mcla_motion_calibration_probe, false);
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  Throws([] { probe::ValidateConfig(); }, "MOTION_TRIGGER_REQUIRES_MOTION");
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
#endif
#undef REJECT_COMPETING
  Accepts("standalone admission restored");
  REXCVAR_SET(mcla_arcade_navigation_probe, false);
  Accepts("legacy START-only admission retained");
  Driver disabled;
  Check(!disabled.ArmMenu(1) && disabled.PollMenu().status == MenuStatus::Failed,
        "disabled menu publishes nothing and fails closed");
  Read(disabled, 0);
  REXCVAR_SET(mcla_arcade_navigation_probe, true);
  Check(!disabled.ArmMenu(1), "enabling flag cannot revive failed driver");
}
void PublicApiAndPrerequisites() {
  auto public_driver = probe::CreateDriver();
  Check(public_driver->Setup() == rex::X_STATUS{0}, "actual driver Setup");
  Check(uint32_t(Read(*public_driver, 0).packet_number) == 1, "initial packet one");
  rex::input::X_INPUT_CAPABILITIES capabilities{};
  Check(public_driver->GetCapabilities(0, 0, &capabilities) == rex::X_RESULT{0} &&
      uint16_t(capabilities.gamepad.buttons) == 0x1210 &&
      capabilities.gamepad.left_trigger == 0 && capabilities.gamepad.right_trigger == 0,
      "Arcade capability only START RB A and no throttle");
  Check(public_driver->GetCapabilities(0, 0, nullptr) == rex::X_RESULT{0x48F}, "null capabilities rejected");
  State sentinel{}; sentinel.packet_number = 123;
  for (uint32_t user : {1u, 2u, 3u, UINT32_MAX})
    Check(public_driver->GetState(user, &sentinel) == rex::X_RESULT{0x48F} &&
        uint32_t(sentinel.packet_number) == 123, "other user untouched and rejected");
  Check(public_driver->GetState(0, nullptr) == rex::X_RESULT{0x48F}, "null state rejected");
  Driver idle;
  Check(!idle.ArmMenu(1) && idle.PollMenu().error == MenuError::MissingStartNeutralAck,
        "menu cannot precede START neutral ACK");
  Read(idle, 0);
  Driver down;
  Need(down.Arm(), "prerequisite START arm");
  Read(down, 0x10);
  Check(!down.ArmMenu(1), "START down ACK alone insufficient");
  down.Fail(); Read(down, 0);
}
void CompleteStart(Driver& driver) {
  Need(driver.Arm(), "START arms once");
  Check(driver.GetState(0, nullptr) == rex::X_RESULT{0x48F}, "null START read rejected");
  Check(!driver.Poll().down_ack_ms, "null read does not ACK START");
  Check(uint32_t(Read(driver, 0x10).packet_number) == 2, "START down packet");
  const auto limit = TestClock::now() + 7s;
  auto state = driver.Poll();
  while (state.status == StartStatus::HoldingStart && TestClock::now() < limit) {
    std::this_thread::sleep_for(5ms); state = driver.Poll();
  }
  Need(state.status == StartStatus::WaitingNeutralAck && !state.neutral_ack_ms,
       "poll releases START but does not invent ACK");
  Check(uint32_t(Read(driver, 0).packet_number) == 3, "START release packet");
  Need(driver.Poll().status == StartStatus::Succeeded && driver.Poll().neutral_ack_ms,
       "actual neutral GetState completes START");
}
void CompleteMenu(Driver& driver) {
  uint32_t expected_packet = 3;
  for (uint32_t sequence = 1; sequence <= 6; ++sequence) {
    if (sequence > 1) std::this_thread::sleep_for(2000ms);
    Need(driver.ArmMenu(sequence), "next menu command arms");
    const auto armed = driver.PollMenu();
    Need(armed.published_ms && armed.release_deadline_ms && armed.neutral_ack_deadline_ms,
         "menu deadlines retained");
    Check(*armed.release_deadline_ms == *armed.published_ms + 200 &&
          *armed.neutral_ack_deadline_ms == *armed.published_ms + 5200,
          "deadlines anchored at publication");
    State other{}; other.packet_number = 987;
    Check(driver.GetState(1, &other) == rex::X_RESULT{0x48F} &&
          uint32_t(other.packet_number) == 987, "other user cannot ACK menu");
    Check(driver.GetState(0, nullptr) == rex::X_RESULT{0x48F} &&
          !driver.PollMenu().down_ack_ms, "null read cannot ACK menu");
    Check(uint32_t(Read(driver, mcla::arcade_input::kButtons[sequence - 1]).packet_number) == ++expected_packet,
          "menu down packet advances");
    Check(uint32_t(Read(driver, mcla::arcade_input::kButtons[sequence - 1]).packet_number) == expected_packet,
          "unchanged menu packet stable");
    const auto limit = TestClock::now() + 1s;
    auto state = driver.PollMenu();
    while (state.status == MenuStatus::Holding && TestClock::now() < limit) {
      std::this_thread::sleep_for(5ms); state = driver.PollMenu();
    }
    Need(state.status == MenuStatus::WaitingNeutralAck && state.released_ms && !state.neutral_ack_ms,
         "independent poll releases menu without ACK");
    Check(uint32_t(Read(driver, 0).packet_number) == ++expected_packet, "menu neutral packet advances");
    Need(driver.PollMenu().status == MenuStatus::Succeeded && driver.PollMenu().neutral_ack_ms,
         "actual neutral read completes menu command");
  }
}
void SixCommands() {
  Driver driver; CompleteStart(driver); CompleteMenu(driver);
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
  Check(!driver.ArmMotion(96) && driver.PollMotion().status == mcla::motion_input::Status::Failed,
        "six menu ACKs alone do not authorize motion without finished receipt");
  Read(driver, 0);
#endif
  Check(!driver.ArmMenu(7) && driver.PollMenu().status == MenuStatus::Failed,
        "no seventh command can publish");
  Read(driver, 0);
  Check(!driver.ArmMenu(1), "finished sequence cannot restart");
}
void CompletionPrerequisites() {
  Driver idle;
  Check(!idle.CompleteArcadeReceipt({}, 10, 6), "counts alone cannot complete Arcade");
  Read(idle, 0);
  for (const auto counts : {std::pair{9u, 6u}, std::pair{10u, 5u}, std::pair{11u, 6u}}) {
    Driver incomplete;
    Check(!incomplete.CompleteArcadeReceipt({}, counts.first, counts.second),
          "incorrect capture or command count fails completion");
    Check(incomplete.PollMenu().status == MenuStatus::Failed, "rejected completion is sticky failure");
  }
  std::stop_source stopped; stopped.request_stop();
  Driver cancelled;
  Check(!cancelled.CompleteArcadeReceipt(stopped.get_token(), 10, 6), "stopped completion rejected");
  Read(cancelled, 0);
}
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
void ArcadeMotion(uint32_t trigger, unsigned failure_mode) {
  using MotionStatus = mcla::motion_input::Status;
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_motion_calibration_trigger, trigger);
  Driver driver; CompleteStart(driver); CompleteMenu(driver);
  // Simulate only RunArcade's post-Finish transition. This guest-free test does
  // not claim that any file was written or any race scene was reached.
  Need(driver.CompleteArcadeReceipt({}, 10, 6), "finished receipt boundary admits motion");
  if (failure_mode == 3) {
    Check(!driver.ArmMotion(97) && driver.PollMotion().status == MotionStatus::Failed,
          "invalid amplitude rejected even after complete Arcade inventory");
    Read(driver, 0);
    Check(!driver.ArmMotion(trigger), "invalid amplitude cannot retry valid amplitude");
    REXCVAR_SET(mcla_motion_calibration_probe, false);
    REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
    return;
  }
  Need(driver.ArmMotion(trigger), "selected exact amplitude arms after complete Arcade inventory");
  Read(driver, 0, static_cast<uint8_t>(trigger));
  const auto armed = driver.PollMotion();
  Need(armed.published_ms && armed.release_deadline_ms && armed.neutral_ack_deadline_ms,
       "motion deadlines retained");
  Check(armed.right_trigger == trigger, "selected amplitude is latched exactly");
  Check(*armed.release_deadline_ms == *armed.published_ms + 3000 &&
        *armed.neutral_ack_deadline_ms == *armed.published_ms + 8000,
        "Arcade motion preserves existing three second hold and five second ACK window");
  if (failure_mode == 2) {
    Check(!driver.ArmMotion(trigger), "rearm while throttle held is rejected");
    Check(driver.PollMotion().status == MotionStatus::Failed, "rearm fails motion closed");
    Read(driver, 0);
  } else if (failure_mode == 1) {
    Check(!driver.ArmMenu(7), "unexpected menu command fails during throttle");
    Check(driver.PollMotion().status == MotionStatus::Failed, "menu failure neutralizes motion via poll");
    Read(driver, 0);
  } else {
    const auto limit = TestClock::now() + 4s;
    auto state = driver.PollMotion();
    while (state.status == MotionStatus::HoldingRightTrigger && TestClock::now() < limit) {
      std::this_thread::sleep_for(5ms); state = driver.PollMotion();
    }
    Need(state.status == MotionStatus::WaitingNeutralAck && !state.neutral_ack_ms,
         "motion poll releases without inventing neutral ACK");
    Read(driver, 0);
    Need(driver.PollMotion().status == MotionStatus::Succeeded, "real neutral read completes Arcade motion");
    Check(!driver.CompleteArcadeReceipt({}, 10, 6), "completion boundary cannot replay");
    Check(driver.PollMotion().status == MotionStatus::Failed, "replayed completion fails motion closed");
    Read(driver, 0);
  }
  Check(!driver.ArmMotion(trigger), "failed or completed motion cannot retry");
  Read(driver, 0);
  REXCVAR_SET(mcla_motion_calibration_probe, false);
  REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
}
#endif
void FailureNeutralization() {
  Driver driver; CompleteStart(driver);
  Need(driver.ArmMenu(1), "failure case menu arms");
  Read(driver, 0x10);
  driver.Fail();
  const auto failed = driver.PollMenu();
  Check(failed.status == MenuStatus::Failed && failed.error == MenuError::ExplicitFailure && failed.released_ms,
        "Fail records terminal neutralization");
  Read(driver, 0);
  Check(!driver.ArmMenu(2) && !driver.Arm(), "failure cannot rearm either latch");
  Read(driver, 0);
}
}  // namespace
int main(int argc, char** argv) {
  try {
    Need(argc == 1 || (argc == 2 && (std::string_view(argv[1]) == "96" ||
         std::string_view(argv[1]) == "255")), "optional exact 96 or 255 amplitude");
    [[maybe_unused]] const uint32_t trigger = argc == 2 && std::string_view(argv[1]) == "255" ? 255 : 96;
    Registry(); PublicApiAndPrerequisites(); SixCommands(); FailureNeutralization(); CompletionPrerequisites();
#if defined(MCLA_MOTION_CALIBRATION_PROBE)
    for (unsigned mode = 0; mode < 4; ++mode) ArcadeMotion(trigger, mode);
#endif
  }
  catch (const std::exception& error) { ++failures; std::cerr << "FATAL " << error.what() << '\n'; }
  catch (...) { ++failures; std::cerr << "FATAL unknown exception\n"; }
  std::cout << "arcade driver: " << checks << " checks, " << failures << " failures\n";
  std::cout.flush(); std::cerr.flush();
  return failures || !std::cout || !std::cerr ? 1 : 0;
}
