#pragma once

// Raw-timer step smoothing (mcla_raw_timer.cpp), moved here unchanged so it
// can be unit-tested (tests/mcla_step_smoother_test.cpp), plus the KI-054
// hitch hold (mcla_raw_timer_hitch_hold; v2 since l85,
// docs/evidence/M11-l85-owner-build-20261110.md; default off until l85, on
// since l86, kHitchHoldDefault).
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
// normal step, which makes the chase camera and the ambient traffic jump.
//
// Hitch hold v2 (KI-054, l85). The l84 v1 held a hitch to 1.1 x an average
// that excluded hitches; on the Steam Deck (12 % hitches, measured mean
// 66-71 ms) that ceiling stayed below the measured mean, the debt sat at the
// cap and 2.2 % of game time was dropped. v2, per update with a measured step
// `raw` (hitch or not):
//   pause:   raw > kPauseSeconds is a loading pause: the previous applied
//            step is applied again, the rest is dropped (not replayed) and
//            counted in paused_seconds(); average and debt are unchanged.
//   average: average += kAverageWeight * (min(raw, kWinsor * average) -
//            average), so the average tracks the true mean frame time,
//            hitches included, while one rare stutter moves it by at most
//            5 % (kAverageWeight * (kWinsor - 1)).
//   target:  average + kRepayment * debt (the same 2 % as without the hold),
//            held within +-kHold of the average, which bounds the catch-up
//            after a stutter to ~1.1x; the band widens linearly to +-2 x
//            kHold between kWideDebtSeconds and twice that debt, so dense
//            hitch bursts are repaid before the debt reaches the cap.
//   applied: the target, moved at most +-kSlew from the previous applied
//            step (no step-to-step ratio above 1.1), and never above the
//            game's 0.1 s clamp.
//   debt:    debt += raw - applied, capped at kMaxHitchDebtSeconds; time
//            above the cap is counted in lost_seconds().
// The first step after a reset applies min(raw, 0.1 s) and starts the average.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mcla::raw_timer {

inline constexpr double kHitchSeconds = 0.1;
inline constexpr double kMaxHitchDebtSeconds = 1.0;
// Hitch hold v2 only: a single measured step above this is a loading pause.
inline constexpr double kPauseSeconds = 0.5;
// The default of the host init-only cvar mcla_raw_timer_hitch_hold: off until
// l85, on since l86 (owner decision 2026-11-10 after the l85 Deck and PC
// runs; docs/evidence/M11-l86-owner-build-20261110.md). The owner control key
// raw_timer_hitch_hold_off (mcla_dev_overrides.h) turns it off for one arm. A
// StepSmoother object itself starts without the hold; the host sets it from
// the cvar.
inline constexpr bool kHitchHoldDefault = true;

class StepSmoother {
 public:
  static constexpr double kAverageWeight = 0.05;
  static constexpr double kRepayment = 0.02;
  static constexpr double kHold = 0.10;
  // Hitch hold v2 (see above).
  static constexpr double kWinsor = 2.0;
  static constexpr double kSlew = 0.10;
  static constexpr double kWideDebtSeconds = 0.25;

  void set_hitch_hold(bool on) { hitch_hold_ = on; }
  bool hitch_hold() const { return hitch_hold_; }

  double Apply(double raw) {
    if (!(raw > 0.0)) {
      Reset();
      return raw;
    }
    if (hitch_hold_) {
      return ApplyHold(raw);
    }
    if (raw > kHitchSeconds) {
      // Apply what the game would clamp to anyway, and keep the rest (and the
      // pending repayment) instead of losing it. The average is left as is.
      const double applied = kHitchSeconds;
      AddDebt(raw - applied);
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
  // Seconds dropped at the debt cap since construction (not reset).
  double lost_seconds() const { return lost_; }
  // Hitch hold v2: seconds dropped as loading pauses since construction.
  double paused_seconds() const { return paused_; }

 private:
  void Reset() {
    has_average_ = false;
    average_ = 0.0;
    debt_ = 0.0;
    last_applied_ = 0.0;
  }
  void AddDebt(double amount) {
    const double next = debt_ + amount;
    if (next > kMaxHitchDebtSeconds) {
      lost_ += next - kMaxHitchDebtSeconds;
    }
    debt_ = std::min(next, kMaxHitchDebtSeconds);
  }
  // The catch-up band: +-kHold while the debt is below kWideDebtSeconds,
  // widening linearly to +-2 x kHold at twice that debt.
  static double HoldBand(double debt) {
    const double wide = std::clamp((debt - kWideDebtSeconds) / kWideDebtSeconds, 0.0, 1.0);
    return kHold * (1.0 + wide);
  }
  double ApplyHold(double raw) {
    if (raw > kHitchSeconds) {
      ++hitches_;
    }
    double applied = 0.0;
    if (!has_average_) {
      applied = std::min(raw, kHitchSeconds);
      average_ = applied;
      has_average_ = true;
      if (raw > kPauseSeconds) {
        paused_ += raw - applied;
      } else {
        AddDebt(raw - applied);
      }
    } else if (raw > kPauseSeconds) {
      applied = last_applied_;
      paused_ += raw - applied;
    } else {
      average_ += kAverageWeight * (std::min(raw, kWinsor * average_) - average_);
      const double band = HoldBand(debt_);
      const double target = std::clamp(average_ + kRepayment * debt_,
                                       (1.0 - band) * average_, (1.0 + band) * average_);
      applied = std::clamp(target, (1.0 - kSlew) * last_applied_,
                           (1.0 + kSlew) * last_applied_);
      applied = std::min(applied, kHitchSeconds);
      AddDebt(raw - applied);
    }
    last_applied_ = applied;
    return applied;
  }
  bool hitch_hold_ = false;
  bool has_average_ = false;
  double average_ = 0.0;
  double debt_ = 0.0;
  double last_applied_ = 0.0;
  double lost_ = 0.0;
  double paused_ = 0.0;
  uint64_t hitches_ = 0;
};

// KI-054 l84: per-window ranges for the "MCLA timer stats" line
// (mcla_raw_timer_stats, dev key raw_timer_stats): applied and measured step
// min / average / max and the updates whose applied step is more than 20 %
// away from the measured one. l85: the largest applied step-to-step ratio
// (max(b / a, a / b) of consecutive applied steps in the window).
inline constexpr double kFarFraction = 0.20;

inline bool FarFromRaw(double raw, double applied) {
  return raw > 0.0 && std::fabs(applied - raw) > kFarFraction * raw;
}

struct StepRange {
  uint64_t count = 0;
  uint64_t far = 0;
  double raw_min = 0.0, raw_max = 0.0, raw_sum = 0.0;
  double out_min = 0.0, out_max = 0.0, out_sum = 0.0;
  double out_last = 0.0, max_ratio = 1.0;

  void Add(double raw, double out) {
    raw_min = count == 0 ? raw : std::min(raw_min, raw);
    raw_max = count == 0 ? raw : std::max(raw_max, raw);
    out_min = count == 0 ? out : std::min(out_min, out);
    out_max = count == 0 ? out : std::max(out_max, out);
    if (count != 0 && out_last > 0.0 && out > 0.0) {
      max_ratio = std::max(max_ratio, std::max(out / out_last, out_last / out));
    }
    out_last = out;
    raw_sum += raw;
    out_sum += out;
    far += FarFromRaw(raw, out) ? 1 : 0;
    ++count;
  }
  double raw_avg() const { return count ? raw_sum / double(count) : 0.0; }
  double out_avg() const { return count ? out_sum / double(count) : 0.0; }
};

}  // namespace mcla::raw_timer
