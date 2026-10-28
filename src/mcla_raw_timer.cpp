// Optional measured-time ("raw") game timer.
//
// Stock MCLA advances game time by a fixed 1/30 s per update (timer mode
// +60 = 1, set by sub_821BD9F8). When the host cannot sustain 30 updates/s the
// whole game runs in slow motion. The game also ships a raw policy
// (sub_821BD9B8) that uses the measured timebase delta, clamped per tick.
// sub_822D2478 picks the policy from its configuration object: +72 == 1
// selects raw, and +88 is an optional cap that is cleared here so raw time is
// not capped at 1/30 s. See docs/evidence/M8-001-timebox-20261025.md.
//
// Default ON (MCLA_RAW_TIMER_DEFAULT=0 builds default OFF). --mcla_raw_timer=false
// keeps the stock fixed timer for A/B comparison.

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

#ifndef MCLA_RAW_TIMER_DEFAULT
#define MCLA_RAW_TIMER_DEFAULT 1
#endif

REXCVAR_DEFINE_BOOL(mcla_raw_timer, MCLA_RAW_TIMER_DEFAULT != 0, "MCLA",
    "Use the game's own measured-time timer instead of fixed 1/30 s steps")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_raw_timer_smoothing, true, "MCLA",
    "Average the measured timer step over recent updates (total time is kept)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

// The race clock appears to add each update's step in whole milliseconds,
// dropping the fraction: the owner measured a loss of about 0.5 ms per update
// against a phone stopwatch (-1.9 % at about 27 ms steps, -3.5 % at about
// 14 ms steps, 2026-10-28). Supplying steps that are whole milliseconds, with
// the remainder carried to the next update, keeps both the race clock and the
// total game time exact.
REXCVAR_DEFINE_BOOL(mcla_raw_timer_ms_steps, true, "MCLA",
    "Round raw timer steps to whole milliseconds, carrying the remainder")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_raw_timer_stats, false, "MCLA",
    "Log measured/applied timer step statistics every 300 updates")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

constexpr uint32_t kOne = 0x3F800000u;     // 1.0f
constexpr uint32_t kThirty = 0x41F00000u;  // 30.0f

bool RawTimerWanted() {
  static const bool wanted = REXCVAR_GET(mcla_raw_timer);
  return wanted;
}

std::atomic<int> logged{0};

// Main game timer object and the guest constant that converts timebase ticks
// to seconds (sub_821BDA90: lfs f0,4368(r9) with r9 = 0x82010000).
constexpr uint32_t kMainTimer = 0x827D7500u;
constexpr uint32_t kTicksToSeconds = 0x82011110u;
// Raw steps above the game's own 0.1 s clamp are hitches (loading, stutters).
// The game drops the time above the clamp; up to kMaxHitchDebtSeconds of it is
// kept as debt and repaid over the following updates, so short stutters during
// races do not make the race clock fall behind real time (owner-measured 1.9 %
// over a 3 minute race on 2026-10-28), while long loading pauses are not
// replayed.
constexpr double kHitchSeconds = 0.1;
constexpr double kMaxHitchDebtSeconds = 1.0;

// Updates alternate irregularly between 2 and 3 vblanks (33/50 ms). Applying
// each measured interval directly makes per-frame motion and motion blur jump
// by up to +-25 %. The applied step follows a slow exponential average of the
// measured steps (time constant about 20 updates), so in steady scenes such as
// the pause menu it is nearly constant, like the stock fixed step. The
// difference to measured time is repaid gently (2 % per update, step held
// within +-10 % of the average), so game time stays within a fraction of a
// second of real time and does not drift. Timer ticks run on a single guest
// thread.
class StepSmoother {
 public:
  double Apply(double raw) {
    if (!(raw > 0.0)) {
      Reset();
      return raw;
    }
    if (raw > kHitchSeconds) {
      // Apply what the game would clamp to anyway, and keep the rest (and the
      // pending repayment) instead of losing it. The average is left as is.
      debt_ = std::min(debt_ + (raw - kHitchSeconds), kMaxHitchDebtSeconds);
      return kHitchSeconds;
    }
    average_ = has_average_ ? average_ + kAverageWeight * (raw - average_) : raw;
    has_average_ = true;
    const double applied = std::clamp(average_ + kRepayment * debt_,
                                      (1.0 - kHold) * average_, (1.0 + kHold) * average_);
    debt_ += raw - applied;
    return applied;
  }
  double debt() const { return debt_; }

 private:
  static constexpr double kAverageWeight = 0.05;
  static constexpr double kRepayment = 0.02;
  static constexpr double kHold = 0.10;
  void Reset() {
    has_average_ = false;
    average_ = 0.0;
    debt_ = 0.0;
  }
  bool has_average_ = false;
  double average_ = 0.0;
  double debt_ = 0.0;
};
StepSmoother smoother;

// Whole-millisecond steps with the remainder carried (see mcla_raw_timer_ms_steps).
// The tiny bias keeps a float conversion of n / 1000 from truncating to n - 1.
class MillisecondSteps {
 public:
  double Apply(double step) {
    const double total = step + carry_;
    const double whole_ms = std::floor(total * 1000.0);
    if (whole_ms < 1.0) {
      carry_ = total;
      return step;
    }
    carry_ = total - whole_ms / 1000.0;
    return whole_ms / 1000.0 + 1.0e-7;
  }

 private:
  double carry_ = 0.0;
};
MillisecondSteps millisecond_steps;

// Optional statistics for checking the effect without a diagnostic build.
struct StepStats {
  uint64_t count = 0;
  double raw_sum = 0, raw_sq = 0, out_sum = 0, out_sq = 0;
  double raw_change = 0, out_change = 0, last_raw = 0, last_out = 0;
  void Add(double raw, double out) {
    if (count) {
      raw_change += std::fabs(raw - last_raw);
      out_change += std::fabs(out - last_out);
    }
    last_raw = raw;
    last_out = out;
    raw_sum += raw;
    raw_sq += raw * raw;
    out_sum += out;
    out_sq += out * out;
    if (++count < 300) return;
    const double n = double(count);
    const auto sd = [n](double sum, double sq) {
      return std::sqrt(std::max(0.0, sq / n - (sum / n) * (sum / n)));
    };
    MCLA_APP_INFO(
        "MCLA timer stats: {} updates, measured mean {:.1f} ms sd {:.1f} ms "
        "step change {:.1f} ms; applied mean {:.1f} ms sd {:.1f} ms step change "
        "{:.1f} ms; total measured {:.3f} s applied {:.3f} s",
        count, 1000 * raw_sum / n, 1000 * sd(raw_sum, raw_sq),
        1000 * raw_change / (n - 1), 1000 * out_sum / n, 1000 * sd(out_sum, out_sq),
        1000 * out_change / (n - 1), raw_sum, out_sum);
    *this = {};
  }
};

StepStats stats;

}  // namespace

// Strong override of the generated weak timer tick. In raw mode the tick takes
// an optional caller-supplied step in f1 (negative means "measure"); a smoothed
// step is supplied there, so the game's own raw path still applies its scale,
// clamps and history updates.
REX_EXTERN(sub_821BDA90) {
  const uint32_t timer = ctx.r3.u32;
  if (RawTimerWanted() && timer == kMainTimer && ctx.f1.f64 < 0.0 &&
      REX_LOAD_U32(timer + 60) == 0 && REX_LOAD_U8(timer + 56) == 0 &&
      REX_LOAD_U8(timer + 58) == 0) {
    const uint64_t last = REX_LOAD_U64(timer + 64);
    const uint64_t now = REX_QUERY_TIMEBASE();
    uint32_t factor_bits = REX_LOAD_U32(kTicksToSeconds);
    float factor = 0.0f;
    std::memcpy(&factor, &factor_bits, sizeof(factor));
    if (last != 0 && now > last && factor > 0.0f) {
      const double raw = double(now - last) * double(factor);
      double applied = REXCVAR_GET(mcla_raw_timer_smoothing) ? smoother.Apply(raw) : raw;
      if (REXCVAR_GET(mcla_raw_timer_ms_steps)) applied = millisecond_steps.Apply(applied);
      if (REXCVAR_GET(mcla_raw_timer_stats)) stats.Add(raw, applied);
      ctx.f1.f64 = applied;
    }
  }
  __imp__sub_821BDA90(ctx, base);
}

// Strong override of the generated weak sub_822D2478; the original body runs
// unchanged after the optional two-word configuration edit.
REX_EXTERN(sub_822D2478) {
  const uint32_t config = ctx.r3.u32;
  const bool raw = RawTimerWanted();
  bool edited = false;
  // Only the exact stock fixed30 configuration is edited.
  if (raw && config != 0 && REX_LOAD_U32(config + 72) == 0 &&
      REX_LOAD_U32(config + 80) == kOne && REX_LOAD_U32(config + 84) == kThirty &&
      REX_LOAD_U32(config + 88) == kThirty && REX_LOAD_U32(config + 92) == 1) {
    REX_STORE_U32(config + 88, 0u);
    REX_STORE_U32(config + 72, 1u);
    edited = true;
  }
  // The configuration routine runs only a few times; log the first calls.
  if (logged.fetch_add(1) < 4) {
    MCLA_APP_INFO("MCLA timer: {} (configuration edit {})",
                  raw ? "raw measured time" : "stock fixed 1/30 s",
                  edited ? "applied" : "not applied");
  }
  __imp__sub_822D2478(ctx, base);
}
