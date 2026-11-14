#include "mcla_garage_control_policy.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace mcla::garage_control;
unsigned checks = 0, failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Expected {
  std::string_view action;
  uint16_t buttons;
  uint8_t lt, rt;
  int16_t rx, ry;
  bool camera;
};
// Independent exact numeric XInput expectations, not copied parser results.
constexpr std::array<Expected, 18> kExpected{{
    {"START", 0x0010, 0, 0, 0, 0, false}, {"BACK", 0x0020, 0, 0, 0, 0, false},
    {"A", 0x1000, 0, 0, 0, 0, false}, {"B", 0x2000, 0, 0, 0, 0, false},
    {"X", 0x4000, 0, 0, 0, 0, false}, {"Y", 0x8000, 0, 0, 0, 0, false},
    {"DPAD_UP", 0x0001, 0, 0, 0, 0, false}, {"DPAD_DOWN", 0x0002, 0, 0, 0, 0, false},
    {"DPAD_LEFT", 0x0004, 0, 0, 0, 0, false}, {"DPAD_RIGHT", 0x0008, 0, 0, 0, 0, false},
    {"LB", 0x0100, 0, 0, 0, 0, false}, {"RB", 0x0200, 0, 0, 0, 0, false},
    {"CAMERA_LEFT", 0, 0, 0, -16384, 0, true}, {"CAMERA_RIGHT", 0, 0, 0, 16384, 0, true},
    {"CAMERA_UP", 0, 0, 0, 0, 16384, true}, {"CAMERA_DOWN", 0, 0, 0, 0, -16384, true},
    {"ZOOM_IN", 0, 0, 128, 0, 0, true}, {"ZOOM_OUT", 0, 128, 0, 0, 0, true},
}};
static_assert(kPulseMilliseconds == 200);
static_assert(ParseAction("CAMERA_LEFT")->thumb_rx == -16384);
static_assert(!ParseAction("CAMERA_LEFT 1"));

void ExactMappings() {
  Check(kPulseMilliseconds == 200, "every action uses bounded fixed200ms pulse");
  unsigned digital = 0, camera = 0;
  for (const auto &expected : kExpected) {
    const auto command = ParseAction(expected.action);
    Check(command.has_value(), "allowlisted action accepted");
    if (!command) continue;
    Check(command->buttons == expected.buttons, "exact button mask");
    Check(command->left_trigger == expected.lt && command->right_trigger == expected.rt, "exact independent triggers");
    Check(command->thumb_rx == expected.rx && command->thumb_ry == expected.ry, "exact right stick direction and magnitude");
    Check(command->thumb_lx == 0 && command->thumb_ly == 0, "left stick always neutral");
    Check(command->camera == expected.camera, "exact camera classification");
    const int active = (command->buttons != 0) + (command->left_trigger != 0) +
        (command->right_trigger != 0) + (command->thumb_rx != 0) + (command->thumb_ry != 0);
    Check(active == 1, "each token changes exactly one input channel");
    Check(!command->camera || command->buttons == 0, "camera never combines with purchase or menu button");
    Check(command->camera || (!command->left_trigger && !command->right_trigger && !command->thumb_rx && !command->thumb_ry),
        "digital commands retain fully neutral analog state");
    if (command->camera) ++camera; else ++digital;
  }
  Check(digital == 12 && camera == 6, "exact legacy and new allowlist sizes");
  const auto neutral = Command{};
  Check(!neutral.buttons && !neutral.left_trigger && !neutral.right_trigger && !neutral.thumb_lx &&
      !neutral.thumb_ly && !neutral.thumb_rx && !neutral.thumb_ry && !neutral.camera, "default command is fully released");
}
void RejectChanges() {
  for (const auto &expected : kExpected) {
    const std::string token(expected.action);
    for (const auto &suffix : {std::string(" "), std::string("\t"), std::string("\r"), std::string("\n"),
        std::string("\r\n"), std::string("\0", 1), std::string("=32767"), std::string(" 60000"), std::string("+A")}) {
      Check(!ParseAction(token + suffix), "whitespace/trailing parameters/embeddedNUL rejected");
      Check(!ParseAction(suffix + token), "leading junk or whitespace rejected");
    }
    auto lower = token;
    for (auto &c : lower) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    Check(!ParseAction(lower), "case folding forbidden");
    auto mixed = token; mixed[0] = static_cast<char>(mixed[0] - 'A' + 'a');
    Check(!ParseAction(mixed), "mixed case rejected");
    for (const auto &other : kExpected) {
      Check(!ParseAction(token + "+" + std::string(other.action)), "combined action tokens rejected");
      Check(!ParseAction(token + " " + std::string(other.action)), "two actions rejected by action parser");
    }
  }
  for (const auto action : {"", "NONE", "RELEASE", "LT", "RT", "LS", "RS", "L3", "R3", "CAMERA", "ZOOM",
      "CAMERA_FORWARD", "CAMERA_LEFT=-32768", "CAMERA_RIGHT:32767", "CAMERA_UP 16384", "ZOOM_IN 255",
      "ZOOM_OUT -1", "1 CAMERA_LEFT", "0x1000", "16384", "A|CAMERA_LEFT", "CAMERA_LEFT,A",
      "CAMERA_LEFT;A", "CAMERA_LEFT\nA", " CAMERA_LEFT", "CAMERA_LEFT\t"})
    Check(!ParseAction(action), "unknown aliases, numeric magnitudes and protocol envelopes rejected");
  Check(!ParseAction(std::string(65536, 'A')), "unbounded caller string does not become a command");
  Check(!ParseAction(std::string("CAMERA_LEFT\0A", 13)), "embeddedNUL cannot hide second action");
}
void PulseLifecycle() {
  enum class Scenario { Normal, StopBefore, StopInSet, StopInHold, StopInRelease,
      SetFalse, HoldFalse, ReleaseFalse, SetThrows, HoldThrows, ReleaseThrows };
  for (const auto scenario : {Scenario::Normal, Scenario::StopBefore, Scenario::StopInSet,
      Scenario::StopInHold, Scenario::StopInRelease, Scenario::SetFalse, Scenario::HoldFalse,
      Scenario::ReleaseFalse, Scenario::SetThrows, Scenario::HoldThrows, Scenario::ReleaseThrows}) {
    std::stop_source stop;
    if (scenario == Scenario::StopBefore) stop.request_stop();
    const auto expected = *ParseAction("CAMERA_RIGHT");
    std::string order;
    unsigned sets = 0, holds = 0, releases = 0;
    const bool result = RunCameraPulse(expected, stop.get_token(),
        [&](const Command &command) {
          order += 'S'; ++sets;
          Check(command == expected, "pulse set receives exact canonical command");
          if (scenario == Scenario::StopInSet) stop.request_stop();
          if (scenario == Scenario::SetThrows) throw std::runtime_error("synthetic set failure");
          return scenario != Scenario::SetFalse;
        },
        [&] {
          order += 'R'; ++releases;
          if (scenario == Scenario::StopInSet || scenario == Scenario::StopInHold)
            Check(stop.stop_requested(), "release still invoked with cancellation active");
          if (scenario == Scenario::StopInRelease) stop.request_stop();
          if (scenario == Scenario::ReleaseThrows) throw std::runtime_error("synthetic release failure");
          return scenario != Scenario::ReleaseFalse;
        },
        [&](uint32_t milliseconds) {
          order += 'H'; ++holds;
          Check(milliseconds == 200, "hold receives only fixed200ms duration");
          Check(!stop.stop_requested(), "hold never begins after observed cancellation");
          if (scenario == Scenario::StopInHold) stop.request_stop();
          if (scenario == Scenario::HoldThrows) throw std::runtime_error("synthetic hold failure");
          return scenario != Scenario::HoldFalse;
        });
    Check(result == (scenario == Scenario::Normal), "pulse succeeds only when all stages succeed without cancellation");
    if (scenario == Scenario::StopBefore) {
      Check(order.empty() && sets == 0 && holds == 0 && releases == 0, "pre-existing cancellation has no callbacks");
    } else {
      Check(sets == 1 && releases == 1, "every attempted set releases exactly once");
      const bool no_hold = scenario == Scenario::StopInSet || scenario == Scenario::SetFalse || scenario == Scenario::SetThrows;
      Check(holds == (no_hold ? 0u : 1u), "hold requires successful uncancelled set");
      Check(order == (no_hold ? "SR" : "SHR"), "exact set-hold-release order retained on all failure paths");
    }
  }
  // Run all six canonical camera commands, not just one direction.
  for (const auto &expected : kExpected) {
    const auto command = *ParseAction(expected.action);
    unsigned callbacks = 0;
    const bool result = RunCameraPulse(command, {}, [&](const Command &actual) {
      ++callbacks; return actual == command;
    }, [&] { ++callbacks; return true; }, [&](uint32_t ms) { ++callbacks; return ms == 200; });
    Check(IsCameraCommand(command) == expected.camera, "canonical validator excludes every digital token");
    Check(result == expected.camera && callbacks == (expected.camera ? 3u : 0u), "only six camera commands may execute");
  }
  const auto valid = *ParseAction("CAMERA_LEFT");
  for (int mutation = 0; mutation < 10; ++mutation) {
    auto invalid = valid;
    if (mutation == 0) invalid.buttons = 0x1000;
    if (mutation == 1) invalid.left_trigger = 128;
    if (mutation == 2) invalid.right_trigger = 128;
    if (mutation == 3) invalid.thumb_lx = 1;
    if (mutation == 4) invalid.thumb_ly = -1;
    if (mutation == 5) invalid.thumb_rx = -32768;
    if (mutation == 6) invalid.thumb_rx = -16383;
    if (mutation == 7) invalid.thumb_ry = 16384;
    if (mutation == 8) invalid.camera = false;
    if (mutation == 9) invalid = Command{};
    unsigned callbacks = 0;
    const auto result = RunCameraPulse(invalid, {}, [&](const Command &) { ++callbacks; return true; },
        [&] { ++callbacks; return true; }, [&](uint32_t) { ++callbacks; return true; });
    Check(!IsCameraCommand(invalid) && !result && callbacks == 0, "caller-mutated noncanonical input rejected before any callback");
  }
}
} // namespace

int main() {
  ExactMappings(); RejectChanges(); PulseLifecycle();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
