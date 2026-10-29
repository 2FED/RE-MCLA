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

using mcla::menu_state::Controller;
using mcla::menu_state::Inputs;
using mcla::menu_state::kForcedLow;
using mcla::menu_state::kMaxLoggedTransitions;

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
  std::printf("mcla_menu_state_test: %d checks passed\n", checks);
  return 0;
}
