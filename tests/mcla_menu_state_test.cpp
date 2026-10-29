#include "mcla_menu_state.h"

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

using mcla::menu_state::CapPaused;
using mcla::menu_state::Controller;
using mcla::menu_state::Inputs;
using mcla::menu_state::kForcedLow;
using mcla::menu_state::kMaxLoggedTransitions;
using mcla::menu_state::kRacerStateActive;
using mcla::menu_state::RacerInputOff;

constexpr Inputs kUnlocked{240, 250};

void TestSteadyStateNeedsNoInputs() {
  Controller c;
  Check(!c.NeedsInputs(false), "running at start needs no cvar reads");
  const auto d = c.Observe(false, kUnlocked);
  Check(!d.transition && !d.log && !d.write_low, "no edge, no action");
  Check(c.NeedsInputs(true), "pause edge needs cvar reads");
}

void TestPauseAndResumeRestoreSavedValue() {
  Controller c;
  auto d = c.Observe(true, kUnlocked);
  Check(d.transition && d.log, "pause edge is a logged transition");
  Check(d.write_low && *d.write_low == kForcedLow, "pause forces the cap");
  Check(c.paused() && c.forcing() && c.saved_low() == 250, "state after pause");
  Check(!c.NeedsInputs(true), "steady pause needs no cvar reads");
  d = c.Observe(true, kUnlocked);
  Check(!d.transition && !d.write_low, "repeated pause is ignored");
  d = c.Observe(false, Inputs{240, kForcedLow});
  Check(d.transition && d.write_low && *d.write_low == 250,
        "resume restores the saved threshold");
  Check(!c.paused() && !c.forcing(), "state after resume");
}

void TestStockModeAndDisabledCapAreLeftAlone() {
  Controller stock;
  auto d = stock.Observe(true, Inputs{0, 250});
  Check(d.transition && !d.write_low && !stock.forcing(), "stock mode: no write");
  d = stock.Observe(false, Inputs{0, 250});
  Check(d.transition && !d.write_low, "stock mode resume: no write");

  Controller disabled;
  d = disabled.Observe(true, Inputs{240, 0});
  Check(!d.write_low && !disabled.forcing(), "UI cap disabled: no write");
  d = disabled.Observe(false, Inputs{240, 0});
  Check(!d.write_low, "UI cap disabled resume: no write");
}

void TestAlreadyForcedValueIsNotWritten() {
  Controller c;
  auto d = c.Observe(true, Inputs{240, kForcedLow});
  Check(c.forcing() && !d.write_low, "value already forced: no write");
  d = c.Observe(false, Inputs{240, kForcedLow});
  Check(!d.write_low, "nothing written, nothing restored");
}

void TestUserChangeWhilePausedIsKept() {
  Controller c;
  c.Observe(true, kUnlocked);
  const auto d = c.Observe(false, Inputs{240, 400});
  Check(d.transition && !d.write_low, "a value changed while paused is kept");
  Check(!c.forcing(), "forcing ends on resume");
}

void TestShutdownRestoresAndStops() {
  Controller c;
  c.Observe(true, kUnlocked);
  auto restore = c.Shutdown(kForcedLow);
  Check(restore && *restore == 250, "shutdown restores a forced value");
  Check(c.stopped() && !c.forcing(), "state after shutdown");
  Check(!c.NeedsInputs(false) && !c.NeedsInputs(true), "stopped ignores edges");
  const auto d = c.Observe(false, kUnlocked);
  Check(!d.transition && !d.write_low, "no action after shutdown");
  Check(!c.Shutdown(kForcedLow), "second shutdown is a no-op");

  Controller idle;
  Check(!idle.Shutdown(250), "shutdown while not forcing writes nothing");
  Controller changed;
  changed.Observe(true, kUnlocked);
  Check(!changed.Shutdown(400), "shutdown keeps a user-changed value");
}

void TestLogBound() {
  Controller c;
  int logged = 0;
  for (int i = 0; i < 2 * kMaxLoggedTransitions + 10; ++i) {
    const auto d = c.Observe(i % 2 == 0, kUnlocked);
    Check(d.transition, "every alternation is a transition");
    logged += d.log ? 1 : 0;
  }
  Check(logged == kMaxLoggedTransitions, "at most 200 transitions are logged");
  Check(c.logged() == kMaxLoggedTransitions, "log counter saturates");
}

void TestRepeatedCyclesKeepOriginalValue() {
  Controller c;
  for (int i = 0; i < 5; ++i) {
    const auto pause = c.Observe(true, kUnlocked);
    Check(pause.write_low && *pause.write_low == kForcedLow, "cycle pause");
    const auto resume = c.Observe(false, Inputs{240, kForcedLow});
    Check(resume.write_low && *resume.write_low == 250, "cycle resume");
  }
}
void TestPauseSignals() {
  for (int sim = 0; sim < 2; ++sim) {
    for (int local = 0; local < 2; ++local) {
      for (int racer = 0; racer < 2; ++racer) {
        const bool expected = sim || local || racer;
        Check(CapPaused(static_cast<uint8_t>(sim), static_cast<uint8_t>(local),
                        static_cast<uint8_t>(racer)) == expected,
              "paused is sim || local || racer-off");
      }
    }
  }
  Check(CapPaused(0, 0, 0xFF), "any non-zero racer flag counts");
  Check(!CapPaused(0, 0, RacerInputOff(true, kRacerStateActive, 0)),
        "active racer with input on keeps the game running");
}

void TestRacerInputOff() {
  Check(RacerInputOff(true, kRacerStateActive, 1) == 1,
        "active deactivated racer reports the flag");
  Check(RacerInputOff(true, kRacerStateActive, 0) == 0,
        "active driving racer reports 0");
  Check(RacerInputOff(false, kRacerStateActive, 1) == 0,
        "refused read reports 0");
  for (const uint32_t state : {0u, 1u, 2u, 4u, 0xFFFFFFFFu}) {
    Check(RacerInputOff(true, state, 1) == 0,
          "inactive racer state ignores the flag");
  }
}

void TestHangoutCycleThroughController() {
  // Hangout menu: sim and local stay 0, the racer is deactivated.
  Controller c;
  const auto enter = c.Observe(CapPaused(0, 0, RacerInputOff(true, 3, 1)),
                               kUnlocked);
  Check(enter.transition && enter.write_low && *enter.write_low == kForcedLow,
        "hangout menu forces the cap");
  Check(!c.NeedsInputs(CapPaused(1, 0, 1)),
        "pause menu over the hangout is no new edge");
  const auto leave = c.Observe(CapPaused(0, 0, RacerInputOff(true, 3, 0)),
                               Inputs{240, kForcedLow});
  Check(leave.write_low && *leave.write_low == 250,
        "reactivated racer restores the threshold");
}
}  // namespace

int main() {
  TestSteadyStateNeedsNoInputs();
  TestPauseAndResumeRestoreSavedValue();
  TestStockModeAndDisabledCapAreLeftAlone();
  TestAlreadyForcedValueIsNotWritten();
  TestUserChangeWhilePausedIsKept();
  TestShutdownRestoresAndStops();
  TestLogBound();
  TestRepeatedCyclesKeepOriginalValue();
  TestPauseSignals();
  TestRacerInputOff();
  TestHangoutCycleThroughController();
  std::printf("mcla_menu_state_test: %d checks passed\n", checks);
  return 0;
}
