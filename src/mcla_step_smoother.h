#pragma once

// Raw-timer step smoothing (mcla_raw_timer.cpp), moved here unchanged so it
// can be unit-tested (tests/mcla_step_smoother_test.cpp), plus the KI-054 H2
// hitch hold (mcla_raw_timer_hitch_hold, default off;
// docs/evidence/M11-ki054-traffic-jitter-design-20261110.md, Arm 2).
//
// Updates alternate irregularly between 2 and 3 vblanks (33/50 ms). Applying
// each measured interval directly makes per-frame motion and motion blur jump
// by up to +-25 %. The applied step follows a slow exponential average of the
// measured steps (time constant about 20 updates), so in steady scenes such as
// the pause menu it is nearly constant, like the stock fixed step. The
// difference to measured time is repaid gently (2 % per update, step held
// within +-10 % of the average), so game time stays within a fraction of a
// second of real time and does not drift. Timer ticks run on a single guest
// thread.
//
// Raw steps above the game's own 0.1 s clamp are hitches (loading, stutters).
// The game drops the time above the clamp; up to kMaxHitchDebtSeconds of it is
// kept as debt and repaid over the following updates, so short stutters during
// races do not make the race clock fall behind real time (owner-measured 1.9 %
// over a 3 minute race on 2026-10-28), while long loading pauses are not
// replayed. Without the hitch hold a hitch applies exactly 0.1 s, 2.5-3x a
// normal step, which makes the chase camera's per-update filters dip for a
// frame. With the hitch hold a hitch applies min(0.1 s, 1.1 x average) (the
// same +10 % bound as the repayment) and the rest goes to the same debt, so
// the step never jumps by more than the hold; the time is repaid by the
// existing +-10 % mechanism. The race clock stays exact as before: every
// second not applied at the hitch is in the debt and is applied later, up to
// the same 1 s debt cap (with the hold the cap is reached about 0.06 s of
// stall sooner per hitch, because more of each hitch is deferred).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mcla::raw_timer {

inline constexpr double kHitchSeconds = 0.1;
inline constexpr double kMaxHitchDebtSeconds = 1.0;

class StepSmoother {
 public:
  static constexpr double kAverageWeight = 0.05;
  static constexpr double kRepayment = 0.02;
  static constexpr double kHold = 0.10;

  void set_hitch_hold(bool on) { hitch_hold_ = on; }
  bool hitch_hold() const { return hitch_hold_; }

  double Apply(double raw) {
    if (!(raw > 0.0)) {
      Reset();
      return raw;
    }
    if (raw > kHitchSeconds) {
      // Apply what the game would clamp to anyway (or, with the hold, at most
      // +10 % over the average), and keep the rest (and the pending
      // repayment) instead of losing it. The average is left as is.
      double applied = kHitchSeconds;
      if (hitch_hold_ && has_average_) {
        applied = std::min(kHitchSeconds, (1.0 + kHold) * average_);
      }
      debt_ = std::min(debt_ + (raw - applied), kMaxHitchDebtSeconds);
      ++hitches_;
      return applied;
    }
    average_ = has_average_ ? average_ + kAverageWeight * (raw - average_) : raw;
    has_average_ = true;
    const double applied = std::clamp(average_ + kRepayment * debt_,
                                      (1.0 - kHold) * average_, (1.0 + kHold) * average_);
    debt_ += raw - applied;
    return applied;
  }
  double debt() const { return debt_; }
  double average() const { return average_; }
  bool has_average() const { return has_average_; }
  // Raw steps above kHitchSeconds since construction (not reset).
  uint64_t hitches() const { return hitches_; }

 private:
  void Reset() {
    has_average_ = false;
    average_ = 0.0;
    debt_ = 0.0;
  }
  bool hitch_hold_ = false;
  bool has_average_ = false;
  double average_ = 0.0;
  double debt_ = 0.0;
  uint64_t hitches_ = 0;
};

// KI-054 l84: per-window ranges for the "MCLA timer stats" line
// (mcla_raw_timer_stats, dev key raw_timer_stats): applied and measured step
// min / average / max and the updates whose applied step is more than 20 %
// away from the measured one.
inline constexpr double kFarFraction = 0.20;

inline bool FarFromRaw(double raw, double applied) {
  return raw > 0.0 && std::fabs(applied - raw) > kFarFraction * raw;
}

struct StepRange {
  uint64_t count = 0;
  uint64_t far = 0;
  double raw_min = 0.0, raw_max = 0.0, raw_sum = 0.0;
  double out_min = 0.0, out_max = 0.0, out_sum = 0.0;

  void Add(double raw, double out) {
    raw_min = count == 0 ? raw : std::min(raw_min, raw);
    raw_max = count == 0 ? raw : std::max(raw_max, raw);
    out_min = count == 0 ? out : std::min(out_min, out);
    out_max = count == 0 ? out : std::max(out_max, out);
    raw_sum += raw;
    out_sum += out;
    far += FarFromRaw(raw, out) ? 1 : 0;
    ++count;
  }
  double raw_avg() const { return count ? raw_sum / double(count) : 0.0; }
  double out_avg() const { return count ? out_sum / double(count) : 0.0; }
};

}  // namespace mcla::raw_timer
