#include "mcla_cursor_autohide.h"

#include <cstdio>
#include <cstdlib>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::cursor::Activity;
using mcla::cursor::AutoHideController;
using mcla::cursor::PadActivityFilter;
using mcla::cursor::PadAxisSource;
using Action = AutoHideController::Action;

void TestPadFilter() {
  PadActivityFilter filter;
  constexpr auto kDelta = PadActivityFilter::kAxisActivityDelta;
  Check(filter.OnButtonDown() == Activity::kPad, "button press is activity");
  Check(filter.OnButtonUp() == Activity::kNone, "button release is not");
  Check(filter.OnHat(0) == Activity::kNone, "centred hat is not activity");
  Check(filter.OnHat(1) == Activity::kPad, "pressed hat is activity");

  // First value only seeds the baseline, even far from centre (pedal rest).
  Check(filter.OnAxis(PadAxisSource::kJoystick, 7, 2, -32768) ==
            Activity::kNone,
        "first axis value seeds baseline");
  Check(filter.OnAxis(PadAxisSource::kJoystick, 7, 2, -32768 + kDelta - 1) ==
            Activity::kNone,
        "sub-threshold pedal noise is ignored");
  Check(filter.OnAxis(PadAxisSource::kJoystick, 7, 2, -32768 + kDelta) ==
            Activity::kPad,
        "pedal press past threshold is activity");
  // The baseline follows the accepted value.
  Check(filter.OnAxis(PadAxisSource::kJoystick, 7, 2, -32768 + kDelta + 100) ==
            Activity::kNone,
        "small change after accepted value is ignored");

  // Idle stick noise around a drifting rest never accumulates.
  Check(filter.OnAxis(PadAxisSource::kGamepad, 3, 0, 1500) == Activity::kNone,
        "stick seed");
  for (int i = 0; i < 100; ++i) {
    const int16_t noise = static_cast<int16_t>(1500 + ((i % 2) ? 1800 : -1800));
    Check(filter.OnAxis(PadAxisSource::kGamepad, 3, 0, noise) ==
              Activity::kNone,
          "idle drift noise is ignored");
  }
  Check(filter.OnAxis(PadAxisSource::kGamepad, 3, 0, 30000) == Activity::kPad,
        "stick deflection is activity");
  Check(filter.OnAxis(PadAxisSource::kGamepad, 3, 0, -30000) == Activity::kPad,
        "full reversal is activity");

  // Axes are keyed by source, device and axis independently.
  Check(filter.OnAxis(PadAxisSource::kJoystick, 3, 0, 30000) == Activity::kNone,
        "joystick view of the same device seeds separately");
  Check(filter.OnAxis(PadAxisSource::kGamepad, 3, 1, 30000) == Activity::kNone,
        "another axis seeds separately");
  Check(filter.OnAxis(PadAxisSource::kGamepad, 4, 0, 30000) == Activity::kNone,
        "another device seeds separately");

  filter.OnDeviceRemoved(3);
  Check(filter.OnAxis(PadAxisSource::kGamepad, 3, 0, 0) == Activity::kNone,
        "removed device baseline is forgotten");

  // Exceeding capacity evicts old slots without failing.
  PadActivityFilter full;
  for (uint32_t i = 0; i < PadActivityFilter::kCapacity * 2; ++i) {
    Check(full.OnAxis(PadAxisSource::kJoystick, i, 0, 0) == Activity::kNone,
          "overflow seeds are accepted");
  }
  Check(full.OnAxis(PadAxisSource::kJoystick,
                    PadActivityFilter::kCapacity * 2 - 1, 0, 32767) ==
            Activity::kPad,
        "most recent slot survives eviction");
}

void TestController() {
  AutoHideController unfocused;
  Check(unfocused.OnPadActivity(true) == Action::kNone,
        "no hide while unfocused");
  Check(!unfocused.hidden(), "unfocused stays visible");

  AutoHideController c(true);
  Check(c.OnMouseActivity(false) == Action::kNone,
        "mouse while visible does nothing");
  Check(c.OnPadActivity(false) == Action::kNone,
        "no hide over another cursor owner");
  Check(c.OnPadActivity(true) == Action::kHide, "pad hides when focused");
  Check(c.hidden(), "hidden after pad");
  Check(c.OnPadActivity(true) == Action::kNone, "repeat pad is idempotent");
  Check(c.OnMouseActivity(true) == Action::kNone,
        "captured mouse motion does not reveal");
  Check(c.hidden(), "hide stays owned during capture");
  Check(c.OnMouseActivity(false) == Action::kShow, "mouse reveals");
  Check(!c.hidden(), "visible after mouse");
  Check(c.OnPadActivity(true) == Action::kHide, "pad hides again");

  Check(c.OnFocusChanged(false) == Action::kShow, "focus loss restores");
  Check(!c.focused() && !c.hidden(), "unfocused and visible");
  Check(c.OnFocusChanged(false) == Action::kNone, "repeat focus loss is a no-op");
  Check(c.OnPadActivity(true) == Action::kNone, "pad ignored while unfocused");
  Check(c.OnMouseActivity(false) == Action::kNone, "mouse ignored when visible");
  Check(c.OnFocusChanged(true) == Action::kNone, "focus gain does not hide");
  Check(c.OnPadActivity(true) == Action::kHide, "pad hides after refocus");

  Check(c.OnShutdown() == Action::kShow, "shutdown restores owned hide");
  Check(c.OnShutdown() == Action::kNone, "second shutdown is a no-op");
  Check(c.OnPadActivity(true) == Action::kNone, "no hide after shutdown");
}
} // namespace

int main() {
  TestPadFilter();
  TestController();
  std::printf("PASS mcla_cursor_autohide_test checks=%d\n", checks);
  return 0;
}
