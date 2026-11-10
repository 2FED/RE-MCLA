// Raw-timer step smoothing (src/mcla_step_smoother.h): the unchanged default
// behaviour and the KI-054 hitch hold v2 (mcla_raw_timer_hitch_hold), with a
// replay harness of synthetic Steam Deck and PC step sequences (l85).

#include "mcla_step_smoother.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

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

using mcla::raw_timer::kHitchHoldDefault;
using mcla::raw_timer::kHitchSeconds;
using mcla::raw_timer::kMaxHitchDebtSeconds;
using mcla::raw_timer::kPauseSeconds;
using mcla::raw_timer::StepSmoother;

// Feeds `count` steps of `raw` and returns the sum of the applied steps.
double Feed(StepSmoother &s, double raw, int count) {
  double sum = 0.0;
  for (int i = 0; i < count; ++i) {
    sum += s.Apply(raw);
  }
  return sum;
}

// The l84 smoother (git 04b8803, src/mcla_step_smoother.h) frozen as the
// reference: hold off must stay identical to it; hold on is the l84 v1 hold
// (min(0.1 s, 1.1 x an average that excludes hitches)), kept for the table.
class L84Smoother {
 public:
  explicit L84Smoother(bool hold) : hold_(hold) {}
  double Apply(double raw) {
    if (!(raw > 0.0)) {
      has_average_ = false;
      average_ = 0.0;
      debt_ = 0.0;
      return raw;
    }
    if (raw > kHitchSeconds) {
      double applied = kHitchSeconds;
      if (hold_ && has_average_) {
        applied = std::min(kHitchSeconds, 1.1 * average_);
      }
      const double next = debt_ + (raw - applied);
      lost_ += std::max(0.0, next - kMaxHitchDebtSeconds);
      debt_ = std::min(next, kMaxHitchDebtSeconds);
      return applied;
    }
    average_ = has_average_ ? average_ + 0.05 * (raw - average_) : raw;
    has_average_ = true;
    const double applied = std::clamp(average_ + 0.02 * debt_, 0.9 * average_, 1.1 * average_);
    debt_ += raw - applied;
    return applied;
  }
  double debt() const { return debt_; }
  double lost_seconds() const { return lost_; }

 private:
  bool hold_;
  bool has_average_ = false;
  double average_ = 0.0;
  double debt_ = 0.0;
  double lost_ = 0.0;
};

// Deterministic generator (splitmix64), independent of the standard library.
class Random {
 public:
  explicit Random(uint64_t seed) : state_(seed) {}
  double Uniform() {  // [0, 1)
    uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return double(z >> 11) * (1.0 / 9007199254740992.0);
  }
  double Uniform(double lo, double hi) { return lo + (hi - lo) * Uniform(); }
  double Gauss(double mean, double sd) {
    const double u = 1.0 - Uniform();
    const double v = Uniform();
    return mean + sd * std::sqrt(-2.0 * std::log(u)) * std::cos(6.283185307179586 * v);
  }
  double Exponential(double mean) { return -mean * std::log(1.0 - Uniform()); }

 private:
  uint64_t state_;
};

// Steam Deck l84 dense windows (D:84, docs/evidence/M11-l84-deck-analysis-
// 20261110.md): measured mean 66-71 ms, sd 35-37 ms, 32-38 hitches per 300
// updates up to 330 ms, min 18-25 ms. Normal updates ~58 +- 12 ms; with
// probability `density` an update is a hitch of 100 ms + exp(50 ms) (at most
// 330 ms), followed with probability `pair` by a short 18-30 ms partner.
std::vector<double> DeckSequence(int count, double density, double pair, uint64_t seed) {
  Random r(seed);
  std::vector<double> out;
  while (int(out.size()) < count) {
    if (r.Uniform() < density) {
      out.push_back(std::min(0.330, 0.100 + r.Exponential(0.050)));
      if (r.Uniform() < pair) out.push_back(r.Uniform(0.018, 0.030));
    } else {
      out.push_back(std::max(0.012, r.Gauss(0.058, 0.012)));
    }
  }
  out.resize(size_t(count));
  return out;
}

// PC locked 30: 1/30 s +- 0.5 ms, a 200 ms hitch every 900 updates.
std::vector<double> PcLockedSequence() {
  Random r(4);
  std::vector<double> out;
  for (int i = 0; i < 6000; ++i) out.push_back(i % 900 == 450 ? 0.200 : r.Gauss(1.0 / 30, 0.0005));
  return out;
}

// PC Unlocked: 8 +- 0.8 ms, a 150 ms stutter every 600 updates.
std::vector<double> PcUnlockedSequence() {
  Random r(3);
  std::vector<double> out;
  for (int i = 0; i < 6000; ++i) {
    out.push_back(i % 600 == 300 ? 0.150 : std::max(0.004, r.Gauss(0.008, 0.0008)));
  }
  return out;
}

// 40 +- 4 ms with a 3 s loading pause at update 600.
std::vector<double> PauseSequence() {
  Random r(5);
  std::vector<double> out;
  for (int i = 0; i < 1200; ++i) {
    out.push_back(i == 600 ? 3.0 : std::max(0.01, r.Gauss(0.040, 0.004)));
  }
  return out;
}

struct Result {
  double measured = 0.0, applied = 0.0, lost = 0.0, paused = 0.0;
  double max_ratio = 1.0;  // consecutive applied steps, after `steady` updates
  double max_debt = 0.0, mean_debt = 0.0, final_debt = 0.0;
  double peak_speed = 0.0;  // applied / `base` on updates that are not hitches
  std::vector<double> steps, debts;
};

template <class Smoother>
Result Replay(Smoother &s, const std::vector<double> &raw, int steady, double base) {
  Result r;
  double last = 0.0;
  for (size_t i = 0; i < raw.size(); ++i) {
    const double a = s.Apply(raw[i]);
    r.measured += raw[i];
    r.applied += a;
    if (int(i) > steady && last > 0.0) {
      r.max_ratio = std::fmax(r.max_ratio, std::fmax(a / last, last / a));
    }
    if (int(i) > steady && raw[i] <= kHitchSeconds && base > 0.0) {
      r.peak_speed = std::fmax(r.peak_speed, a / base);
    }
    last = a;
    r.max_debt = std::fmax(r.max_debt, s.debt());
    r.mean_debt += s.debt() / double(raw.size());
    r.steps.push_back(a);
    r.debts.push_back(s.debt());
  }
  r.final_debt = s.debt();
  r.lost = s.lost_seconds();
  return r;
}

Result ReplayOff(const std::vector<double> &raw, int steady, double base) {
  StepSmoother s;
  return Replay(s, raw, steady, base);
}
Result ReplayV1(const std::vector<double> &raw, int steady, double base) {
  L84Smoother s(true);
  return Replay(s, raw, steady, base);
}
Result ReplayV2(const std::vector<double> &raw, int steady, double base) {
  StepSmoother s;
  s.set_hitch_hold(true);
  Result r = Replay(s, raw, steady, base);
  r.paused = s.paused_seconds();
  Check(Near(r.applied + s.debt() + r.lost + r.paused, r.measured, 1e-9),
        "v2: applied + debt + lost + paused == measured");
  return r;
}

void PrintRow(const char *name, const char *arm, const Result &r) {
  std::printf("  %-26s %-3s max_ratio %.3f lost %.3f s (%.2f %%) paused %.3f s max_debt %.3f "
              "mean_debt %.3f peak_speed %.3f\n",
              name, arm, r.max_ratio, r.lost, 100.0 * r.lost / r.measured, r.paused, r.max_debt,
              r.mean_debt, r.peak_speed);
}

void TestDefaults() {
  static_assert(kHitchSeconds == 0.1 && kMaxHitchDebtSeconds == 1.0 && kPauseSeconds == 0.5);
  static_assert(StepSmoother::kAverageWeight == 0.05 && StepSmoother::kRepayment == 0.02 &&
                StepSmoother::kHold == 0.10);
  static_assert(StepSmoother::kWinsor == 2.0 && StepSmoother::kSlew == 0.10 &&
                StepSmoother::kWideDebtSeconds == 0.25);
  // l86: the host cvar mcla_raw_timer_hitch_hold is on by default (owner
  // decision 2026-11-10); the owner control key raw_timer_hitch_hold_off turns
  // it off for one arm. The object itself starts without the hold.
  static_assert(kHitchHoldDefault, "hitch hold default on since l86");
  Check(kHitchHoldDefault, "KI-054 hitch hold host default on (l86)");
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
  Check(s.lost_seconds() == 0.0 && s.paused_seconds() == 0.0, "nothing lost or paused");
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
  // Debt cap; the time above it is counted as lost.
  const double before = s.debt();
  Check(s.Apply(5.0) == kHitchSeconds && Near(s.debt(), kMaxHitchDebtSeconds),
        "the debt is capped at 1 s");
  Check(Near(s.lost_seconds(), before + 4.9 - kMaxHitchDebtSeconds, 1e-12) &&
            s.paused_seconds() == 0.0,
        "without the hold the time above the cap is lost (no pause handling)");
  // Hold before any average: 0.1 s (nothing to hold to).
  StepSmoother first;
  first.set_hitch_hold(true);
  Check(first.Apply(0.300) == kHitchSeconds && Near(first.debt(), 0.200),
        "hold without an average applies 0.1 s");
  StepSmoother paused_first;
  paused_first.set_hitch_hold(true);
  Check(paused_first.Apply(2.0) == kHitchSeconds && paused_first.debt() == 0.0 &&
            Near(paused_first.paused_seconds(), 1.9),
        "hold: a first step above 0.5 s is a pause, not debt");
}

void TestHitchHold() {
  StepSmoother s;
  s.set_hitch_hold(true);
  Check(s.hitch_hold(), "hold on");
  Feed(s, 0.040, 100);
  const double avg = s.average();
  Check(Near(avg, 0.040, 1e-9) && Near(s.debt(), 0.0, 1e-9), "average 40 ms, no debt");
  const double applied = s.Apply(0.250);
  Check(Near(s.average(), avg + 0.05 * (2.0 * avg - avg), 1e-12),
        "v2: a hitch enters the average, winsorized at 2 x the average");
  Check(applied <= 1.1 * 0.040 + 1e-12 && applied > 0.040,
        "v2: a hitch moves the applied step by at most +10 % (44 ms, not 100 ms)");
  Check(Near(s.debt(), 0.250 - applied, 1e-9), "the rest of the hitch is debt");
  Check(s.hitches() == 1, "one hitch counted");
  // A large average: the hold never exceeds the game's 0.1 s clamp.
  StepSmoother big;
  big.set_hitch_hold(true);
  Feed(big, 0.098, 100);
  double big_max = 0.0;
  for (int i = 0; i < 50; ++i) big_max = std::fmax(big_max, big.Apply(i % 2 ? 0.300 : 0.098));
  Check(big_max <= kHitchSeconds, "hold capped at 0.1 s");
  // A pause is not replayed: the previous step is applied again, the debt is
  // unchanged.
  StepSmoother p;
  p.set_hitch_hold(true);
  Feed(p, 0.040, 100);
  const double debt0 = p.debt();
  const double last = p.Apply(0.040);
  Check(p.Apply(3.0) == last && Near(p.debt(), debt0, 1e-9) && Near(p.paused_seconds(), 2.96, 1e-9),
        "v2: a 3 s step applies the previous step and drops the rest as a pause");
  Check(p.lost_seconds() == 0.0, "a pause is not lost at the cap");
  // A 0.5 s step is still a hitch (debt), not a pause.
  StepSmoother edge;
  edge.set_hitch_hold(true);
  Feed(edge, 0.040, 100);
  edge.Apply(kPauseSeconds);
  Check(edge.paused_seconds() == 0.0 && edge.debt() > 0.45, "0.5 s is a hitch, kept as debt");
  // Slew from a small first step: no jump, the debt is repaid.
  StepSmoother start;
  start.set_hitch_hold(true);
  double prev = start.Apply(0.012);
  for (int i = 0; i < 400; ++i) {
    const double a = start.Apply(0.060);
    Check(a <= 1.1 * prev + 1e-12 && a >= 0.9 * prev - 1e-12, "v2 start-up: slew-limited");
    prev = a;
  }
  Check(std::fabs(start.debt()) < 0.005 && Near(prev, 0.060, 1e-3),
        "v2 start-up: the debt is repaid and the step reaches 60 ms");
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
  Check(max_jump_plain >= 0.059 && max_jump_hold <= 0.0050,
        "the hold removes the 0.1 s step (jump 60 ms -> at most 10 % of a ~45 ms step)");
  Check(hold.hitches() == 1 && plain.hitches() == 1, "hitch counts");
}

// l85 replay harness: off (unchanged, must equal l84), v1 (l84 hold, frozen
// reference) and v2 on synthetic Deck and PC sequences.
void TestOffEqualsL84(const std::vector<double> &raw) {
  StepSmoother now;
  L84Smoother l84(false);
  for (const double x : raw) {
    const double a = now.Apply(x);
    const double b = l84.Apply(x);
    Check(a == b && now.debt() == l84.debt() && now.lost_seconds() == l84.lost_seconds(),
          "hold off: identical to the l84 smoother (bit for bit)");
  }
}

void TestReplayDeck() {
  std::printf("mcla_step_smoother_test: replay (steady state after 300 updates)\n");
  for (const double density : {0.04, 0.08, 0.12}) {
    for (const double pair : {0.0, 0.7, 1.0}) {
      for (const uint64_t seed : {1u, 2u, 3u}) {
        const std::vector<double> raw = DeckSequence(3000, density, pair, seed);
        TestOffEqualsL84(raw);
        const Result off = ReplayOff(raw, 300, 0.0);
        const Result v1 = ReplayV1(raw, 300, 0.0);
        const Result v2 = ReplayV2(raw, 300, 0.0);
        Check(v2.max_ratio <= 1.10 + 1e-9, "Deck v2: step-to-step ratio at most 1.10");
        Check(v2.lost < 0.003 * v2.measured && v2.paused == 0.0, "Deck v2: lost < 0.3 %");
        Check(v2.max_debt < 0.8 && v2.mean_debt < 0.2, "Deck v2: debt below the cap, mean < 0.2 s");
        Check(off.max_ratio > 1.4, "Deck off: a hitch applies a full 0.1 s step");
        if (density >= 0.12) {
          Check(v1.lost > 0.01 * v1.measured, "Deck v1: dense hitches lose > 1 % (l84 finding)");
        }
        if (seed == 1) {
          char name[64];
          std::snprintf(name, sizeof(name), "deck d%.2f pair%.1f", density, pair);
          PrintRow(name, "off", off);
          PrintRow(name, "v1", v1);
          PrintRow(name, "v2", v2);
        }
      }
    }
  }
}

void TestReplayPc() {
  const std::vector<double> locked = PcLockedSequence();
  TestOffEqualsL84(locked);
  const Result lo = ReplayOff(locked, 300, 1.0 / 30);
  const Result l1 = ReplayV1(locked, 300, 1.0 / 30);
  const Result l2 = ReplayV2(locked, 300, 1.0 / 30);
  PrintRow("pc locked30 200ms/900", "off", lo);
  PrintRow("pc locked30 200ms/900", "v1", l1);
  PrintRow("pc locked30 200ms/900", "v2", l2);
  Check(l2.max_ratio <= 1.10 + 1e-9 && l2.lost == 0.0 && l2.peak_speed < 1.17,
        "PC locked 30 v2: no jump, nothing lost, catch-up at most ~1.15x");
  // Between hitches v2 is as smooth as off: same applied steps once repaid.
  double off_change = 0.0;
  double v2_change = 0.0;
  for (size_t i = 1; i < locked.size(); ++i) {
    if (i % 900 < 700) continue;  // the quiet part of every 900-update cycle
    off_change += std::fabs(lo.steps[i] - lo.steps[i - 1]);
    v2_change += std::fabs(l2.steps[i] - l2.steps[i - 1]);
  }
  std::printf("  pc locked30 quiet step change: off %.4f ms v2 %.4f ms per update\n",
              1000 * off_change / 1200, 1000 * v2_change / 1200);
  Check(v2_change <= 1.05 * off_change, "PC locked 30 v2: as smooth as off between hitches");

  const std::vector<double> unlocked = PcUnlockedSequence();
  TestOffEqualsL84(unlocked);
  const Result uo = ReplayOff(unlocked, 300, 0.008);
  const Result u1 = ReplayV1(unlocked, 300, 0.008);
  const Result u2 = ReplayV2(unlocked, 300, 0.008);
  PrintRow("pc unlocked8 150ms/600", "off", uo);
  PrintRow("pc unlocked8 150ms/600", "v1", u1);
  PrintRow("pc unlocked8 150ms/600", "v2", u2);
  Check(u2.max_ratio <= 1.10 + 1e-9 && u2.lost == 0.0, "PC Unlocked v2: no jump, nothing lost");
  Check(u2.peak_speed < 1.20, "PC Unlocked v2: catch-up after a stutter at most ~1.15x");
  int repay = 0;
  for (int i = 300; i < 900 && u2.debts[size_t(i)] > 0.005; ++i) ++repay;
  std::printf("  pc unlocked8 v2: stutter debt below 5 ms after %d updates\n", repay);
  Check(repay < 300, "PC Unlocked v2: a stutter is repaid before the next one");

  const std::vector<double> pause = PauseSequence();
  TestOffEqualsL84(pause);
  const Result po = ReplayOff(pause, 300, 0.040);
  const Result p1 = ReplayV1(pause, 300, 0.040);
  const Result p2 = ReplayV2(pause, 300, 0.040);
  PrintRow("pause 3s at 40ms", "off", po);
  PrintRow("pause 3s at 40ms", "v1", p1);
  PrintRow("pause 3s at 40ms", "v2", p2);
  const auto repay_updates = [](const Result &r) {
    int n = 0;
    for (size_t i = 600; i < r.debts.size() && std::fabs(r.debts[i]) > 0.01; ++i) ++n;
    return n;
  };
  std::printf("  pause: updates until |debt| < 10 ms: off %d v1 %d v2 %d\n", repay_updates(po),
              repay_updates(p1), repay_updates(p2));
  Check(Near(po.debts[600], kMaxHitchDebtSeconds) && repay_updates(po) > 200,
        "pause off: 1 s of the pause is kept and replayed (unchanged)");
  Check(repay_updates(p2) == 0 && p2.paused > 2.9 && p2.lost == 0.0 && p2.max_ratio <= 1.10 + 1e-9,
        "pause v2: not replayed, no debt, no jump");
}

// l84/l85: the per-window ranges appended to the "MCLA timer stats" line.
void TestStepRange() {
  using mcla::raw_timer::FarFromRaw;
  using mcla::raw_timer::StepRange;
  Check(!FarFromRaw(0.040, 0.040) && !FarFromRaw(0.040, 0.047) && FarFromRaw(0.040, 0.0490) &&
            FarFromRaw(0.400, 0.100) && !FarFromRaw(0.0, 0.040),
        "far: more than 20 % away from a positive measured step");
  StepRange r;
  Check(r.raw_avg() == 0.0 && r.out_avg() == 0.0 && r.max_ratio == 1.0, "empty range");
  r.Add(0.040, 0.040);
  r.Add(0.050, 0.044);
  r.Add(0.400, 0.100);
  r.Add(0.030, 0.040);
  Check(r.count == 4 && r.far == 2 && Near(r.raw_min, 0.030) && Near(r.raw_max, 0.400) &&
            Near(r.out_min, 0.040) && Near(r.out_max, 0.100) &&
            Near(r.raw_avg(), 0.130, 1e-12) && Near(r.out_avg(), 0.056, 1e-12),
        "min / avg / max of measured and applied steps, far count");
  Check(Near(r.max_ratio, 0.100 / 0.040, 1e-12), "max_ratio: largest up or down step ratio");
  // Over a smoothed run the hitch is the only far update with the plain
  // smoother; the hold keeps the applied step near the average.
  StepSmoother plain;
  StepRange range;
  for (int i = 0; i < 300; ++i) {
    const double raw = (i == 150) ? 0.400 : 0.040;
    range.Add(raw, plain.Apply(raw));
  }
  Check(range.far == 1 && Near(range.out_max, kHitchSeconds), "a run with one hitch");
  Check(Near(range.max_ratio, 2.5, 1e-9), "off: max_ratio 2.5 at the hitch (100 / 40 ms)");
  StepSmoother hold;
  hold.set_hitch_hold(true);
  StepRange held;
  for (int i = 0; i < 300; ++i) held.Add(0.040, hold.Apply(i == 150 ? 0.400 : 0.040));
  Check(held.max_ratio <= 1.10 + 1e-9, "v2: max_ratio at most 1.10");
}

}  // namespace

int main() {
  TestDefaults();
  TestHitchDefault();
  TestHitchHold();
  TestTotalTimeKept();
  TestReplayDeck();
  TestReplayPc();
  TestStepRange();
  std::printf("mcla_step_smoother_test: %d checks passed\n", checks);
  return 0;
}
