// Raw-timer step smoothing (src/mcla_step_smoother.h): the unchanged default
// behaviour and the KI-054 hitch hold (mcla_raw_timer_hitch_hold).

#include "mcla_step_smoother.h"

#include <cmath>
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

bool Near(double a, double b, double tolerance = 1e-12) {
  return std::fabs(a - b) <= tolerance;
}

using mcla::raw_timer::kHitchSeconds;
using mcla::raw_timer::kMaxHitchDebtSeconds;
using mcla::raw_timer::StepSmoother;

// Feeds `count` steps of `raw` and returns the sum of the applied steps.
double Feed(StepSmoother &s, double raw, int count) {
  double sum = 0.0;
  for (int i = 0; i < count; ++i) {
    sum += s.Apply(raw);
  }
  return sum;
}

void TestDefaults() {
  static_assert(kHitchSeconds == 0.1 && kMaxHitchDebtSeconds == 1.0);
  static_assert(StepSmoother::kAverageWeight == 0.05 && StepSmoother::kRepayment == 0.02 &&
                StepSmoother::kHold == 0.10);
  StepSmoother s;
  Check(!s.hitch_hold() && !s.has_average() && s.debt() == 0.0, "default: no hold, empty");
  Check(s.Apply(0.040) == 0.040 && s.has_average() && Near(s.average(), 0.040),
        "the first step is applied as measured and starts the average");
  Check(Near(s.debt(), 0.0), "no debt after the first step");
  // Steady 40 ms: applied stays 40 ms.
  Check(Near(Feed(s, 0.040, 50), 50 * 0.040, 1e-9) && Near(s.debt(), 0.0, 1e-9),
        "steady steps are applied unchanged");
  // Alternating 33/50 ms: applied held within +-10 % of the average.
  for (int i = 0; i < 200; ++i) {
    const double raw = (i % 3 == 0) ? 0.050 : 0.033;
    const double applied = s.Apply(raw);
    Check(applied >= 0.9 * s.average() - 1e-12 && applied <= 1.1 * s.average() + 1e-12,
          "applied within +-10 % of the average");
  }
  // Non-positive raw resets.
  Check(s.Apply(0.0) == 0.0 && !s.has_average() && s.debt() == 0.0, "raw 0 resets");
  Check(s.Apply(-1.0) == -1.0 && !s.has_average(), "negative raw resets and passes through");
}

void TestHitchDefault() {
  StepSmoother s;
  Feed(s, 0.040, 100);
  const double debt0 = s.debt();
  const double avg0 = s.average();
  Check(s.Apply(0.250) == kHitchSeconds, "without the hold a hitch applies exactly 0.1 s");
  Check(Near(s.debt(), debt0 + 0.150, 1e-12), "the rest of the hitch goes to the debt");
  Check(Near(s.average(), avg0), "a hitch leaves the average as is");
  Check(s.hitches() == 1, "one hitch counted");
  // Debt cap.
  Check(s.Apply(5.0) == kHitchSeconds && Near(s.debt(), kMaxHitchDebtSeconds),
        "the debt is capped at 1 s");
  // Hold before any average: 0.1 s (nothing to hold to).
  StepSmoother first;
  first.set_hitch_hold(true);
  Check(first.Apply(0.300) == kHitchSeconds && Near(first.debt(), 0.200),
        "hold without an average applies 0.1 s");
}

void TestHitchHold() {
  StepSmoother s;
  s.set_hitch_hold(true);
  Check(s.hitch_hold(), "hold on");
  Feed(s, 0.040, 100);
  const double avg = s.average();
  const double debt0 = s.debt();
  Check(Near(avg, 0.040, 1e-9), "average 40 ms");
  const double applied = s.Apply(0.250);
  Check(Near(applied, 1.1 * avg) && applied < kHitchSeconds,
        "with the hold a hitch applies 1.1 x the average (44 ms, not 100 ms)");
  Check(Near(s.debt(), debt0 + 0.250 - applied, 1e-12), "the rest of the hitch is debt");
  Check(Near(s.average(), avg), "the average is left as is");
  // A large average: the hold never exceeds the game's 0.1 s clamp.
  StepSmoother big;
  big.set_hitch_hold(true);
  Feed(big, 0.095, 100);
  Check(big.Apply(0.300) == kHitchSeconds, "hold capped at 0.1 s");
  // The step after the hitch stays within +10 % of the average (no lurch).
  const double next = s.Apply(0.040);
  Check(next <= 1.1 * s.average() + 1e-12 && next >= s.average(),
        "repayment after the hitch at most +10 %");
}

void TestTotalTimeKept() {
  // Same raw sequence with and without the hold: applied + debt == raw while
  // under the debt cap, and the debt is repaid over the following updates.
  for (const bool hold : {false, true}) {
    StepSmoother s;
    s.set_hitch_hold(hold);
    double raw_sum = 0.0;
    double applied_sum = 0.0;
    for (int i = 0; i < 600; ++i) {
      double raw = (i % 3 == 0) ? 0.050 : 0.033;
      if (i == 100 || i == 300) {
        raw = 0.400;  // stutters
      }
      if (i == 200) {
        raw = 0.120;
      }
      raw_sum += raw;
      applied_sum += s.Apply(raw);
      Check(Near(applied_sum + s.debt(), raw_sum, 1e-9), "applied + debt == measured");
    }
    Check(std::fabs(s.debt()) < 0.01, "the hitch debt is repaid within the run");
  }
  // With the hold the largest step change at a hitch is the +10 % bound.
  StepSmoother hold;
  hold.set_hitch_hold(true);
  StepSmoother plain;
  double last_hold = 0.0;
  double last_plain = 0.0;
  double max_jump_hold = 0.0;
  double max_jump_plain = 0.0;
  for (int i = 0; i < 300; ++i) {
    const double raw = (i == 150) ? 0.400 : 0.040;
    const double a = hold.Apply(raw);
    const double b = plain.Apply(raw);
    if (i > 0) {
      max_jump_hold = std::fmax(max_jump_hold, std::fabs(a - last_hold));
      max_jump_plain = std::fmax(max_jump_plain, std::fabs(b - last_plain));
    }
    last_hold = a;
    last_plain = b;
  }
  Check(max_jump_plain >= 0.059 && max_jump_hold <= 0.0041,
        "the hold removes the 0.1 s step (jump 60 ms -> at most 4 ms)");
  Check(hold.hitches() == 1 && plain.hitches() == 1, "hitch counts");
}

}  // namespace

int main() {
  TestDefaults();
  TestHitchDefault();
  TestHitchHold();
  TestTotalTimeKept();
  std::printf("mcla_step_smoother_test: %d checks passed\n", checks);
  return 0;
}
