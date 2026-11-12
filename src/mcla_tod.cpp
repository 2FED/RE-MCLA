// M11 time of day (l93; MCLA_TOD v=1). See mcla_tod.h for the guest layout
// and its generated-code citations, docs/evidence/M11-time-of-day-re-20261112.md
// (RE and design) and docs/evidence/M11-l93-time-of-day-control-20261112.md.
//
// Two strong overrides; both call the ORIGINAL:
//   sub_822F1DB0 (time-of-day update, once per frame from sub_823045B0):
//     stage 0, always on and read-only: after the original, reads the hour,
//     rate, auto byte, warp flag and target, the pause state and the night
//     thresholds of *0x8287E26C (pointer checked first), and writes MCLA_TOD
//     lines (first update, one per real minute, every night flag change and
//     warp start / end). With mcla_tod_rate != 1 (default 1, off) it also
//     reads the hour and the warp flag before the original and, when the gate
//     allows (an Arcade preset warp was seen this session, auto on, no warp
//     before or after, not paused, an ordinary step), writes the fixed-up hour
//     to +0x38CC and to the render slot the game just published.
//   sub_822F1518 (warp start): records the caller and, from the Arcade race
//     preset (return 0x8265D61C) only, sets the gate marker; with
//     mcla_tod_start_minutes != 0 (default 0, off) it replaces f1 (the target
//     hour) there before the original runs. Other callers are untouched.
// With both cvars at their defaults no guest memory or register is written.

#include "mcla_tod.h"

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>

REXCVAR_DEFINE_INT32(mcla_tod_rate, 1, "MCLA",
    "Time-of-day rate after an Arcade race preset warp: 1 = the game's own, "
    "10 / 30 = that many times faster, 0 = frozen (dev control; MCLA_TOD_CTL)")
    .range(0, 60)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_INT32(mcla_tod_start_minutes, 0, "MCLA",
    "Replace the Arcade race preset time of day with this many minutes after "
    "midnight (270 = 04:30, 330 = 05:30; 0 = off; dev control)")
    .range(0, 1439)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace tod = mcla::tod;

int RateFactor() {
  static const int factor = tod::RateFactor(REXCVAR_GET(mcla_tod_rate));
  return factor;
}

int StartMinutes() {
  static const int minutes = tod::StartMinutes(REXCVAR_GET(mcla_tod_start_minutes));
  return minutes;
}

float AsFloat(uint32_t bits) {
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

uint32_t AsBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

uint64_t NowMs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

struct Shared {
  // Written by the warp hook (any thread), read by the update hook.
  std::atomic<bool> arcade_seen{false};
  std::atomic<uint32_t> warp_lr{0};
  std::atomic<uint64_t> warp_calls{0};
  std::atomic<uint64_t> preset_warps{0};
  std::atomic<uint64_t> start_replaced{0};
  // Published by the update hook for F10 (under the mutex) on the first
  // update, every 16th and every line.
  std::mutex mutex;
  bool valid = false;
  tod::Sample sample;
  tod::Counters counters;  // updates, lines, edges, acts, blocked
  // Update-thread only (sub_822F1DB0 has one caller, the update task).
  tod::Scheduler scheduler;
  tod::Counters local;
  uint64_t calls = 0;
  uint64_t now_ms = 0;
  bool act_logged = false;
  bool block_logged = false;
};

Shared &State() {
  static Shared state;
  return state;
}

// The control part of the update hook: returns true when it wrote the hour.
bool Control(uint8_t *base, Shared &s, uint32_t manager, float before, bool warp_before,
             tod::Sample &sample) {
  const int factor = RateFactor();
  const bool conditions =
      tod::MayAct(sample.auto_on != 0, warp_before, sample.warp != 0, sample.paused != 0);
  const tod::Gate gate =
      tod::Decide(true, conditions, s.arcade_seen.load(std::memory_order_acquire));
  if (gate == tod::Gate::kIdle) {
    return false;
  }
  if (gate == tod::Gate::kBlocked) {
    ++s.local.blocked;
    if (!s.block_logged) {
      s.block_logged = true;
      MCLA_APP_INFO("MCLA_TOD_CTL v=1 kind=gate_blocked ctl={} hour={:06.3f} "
                    "(no Arcade preset warp seen in this session; the control stays off)",
                    tod::RateLabel(factor), double(sample.hour));
    }
    return false;
  }
  const std::optional<float> fixed = tod::FixUp(factor, before, sample.hour);
  const uint32_t index = REX_LOAD_U32(tod::kUpdateSlotIndex);
  if (!fixed || !tod::SlotIndexValid(index)) {
    return false;
  }
  const uint32_t bits = AsBits(*fixed);
  REX_STORE_U32(manager + tod::kHourOffset, bits);
  // As the game publishes it (`stfsx f0,r9,r31` with r9 = 4 * (3633 + index)).
  REX_STORE_U32(tod::RenderSlotAddress(manager, index), bits);
  const float after = sample.hour;
  sample.hour = *fixed;
  ++s.local.acts;
  if (!s.act_logged) {
    s.act_logged = true;
    MCLA_APP_INFO("MCLA_TOD_CTL v=1 kind=act ctl={} before={:06.3f} after={:06.3f} "
                  "written={:06.3f} slot={}",
                  tod::RateLabel(factor), double(before), double(after), double(*fixed),
                  index);
  }
  return true;
}

}  // namespace

// Time-of-day update: r3 = the lighting manager (*0x8287E26C).
REX_EXTERN(sub_822F1DB0) {
  const uint32_t manager_in = ctx.r3.u32;
  const bool control_on = RateFactor() != tod::kRateOff;
  bool pre = false;
  float before = 0.0f;
  bool warp_before = true;
  if (control_on && tod::PointerPlausible(manager_in)) {
    pre = true;
    before = AsFloat(REX_LOAD_U32(manager_in + tod::kHourOffset));
    warp_before = REX_LOAD_U8(manager_in + tod::kWarpFlagOffset) != 0;
  }
  __imp__sub_822F1DB0(ctx, base);

  const uint32_t manager = REX_LOAD_U32(tod::kManagerPtr);
  if (!tod::PointerPlausible(manager)) {
    return;
  }
  tod::Sample sample;
  sample.hour = AsFloat(REX_LOAD_U32(manager + tod::kHourOffset));
  sample.rate = AsFloat(REX_LOAD_U32(manager + tod::kRateOffset));
  sample.target = AsFloat(REX_LOAD_U32(manager + tod::kWarpTargetOffset));
  sample.night_low = AsFloat(REX_LOAD_U32(manager + tod::kNightLowOffset));
  sample.night_high = AsFloat(REX_LOAD_U32(manager + tod::kNightHighOffset));
  sample.auto_on = REX_LOAD_U8(manager + tod::kAutoOffset);
  sample.warp = REX_LOAD_U8(manager + tod::kWarpFlagOffset);
  const uint32_t pause = REX_LOAD_U32(tod::kPauseState);
  if (tod::PointerPlausible(pause)) {
    sample.paused = tod::Paused(REX_LOAD_U8(pause + tod::kPauseActiveOffset),
                                REX_LOAD_U8(pause + tod::kPauseReleaseOffset))
                        ? 1
                        : 0;
  }
  auto &s = State();
  if (pre && manager == manager_in) {
    Control(base, s, manager, before, warp_before, sample);
  }
  sample.night = tod::NightState(sample.hour, sample.night_low, sample.night_high);

  // The clock is read on the first update and every 16th after it.
  const bool tick = s.calls++ % 16 == 0;
  if (tick) {
    s.now_ms = NowMs();
  }
  const uint8_t why = s.scheduler.Observe(sample, s.now_ms);
  ++s.local.updates;
  s.local.night_changes += (why & tod::kWhyNight) != 0 ? 1 : 0;
  s.local.warp_starts += (why & tod::kWhyWarpStart) != 0 ? 1 : 0;
  std::string line;
  if (why != tod::kWhyNone) {
    ++s.local.lines;
    line = tod::Line(why, sample, tod::RateLabel(RateFactor()), tod::StartLabel(StartMinutes()),
                     s.arcade_seen.load(std::memory_order_relaxed),
                     s.warp_lr.load(std::memory_order_relaxed), s.local.acts);
  }
  if (tick || !line.empty()) {
    std::lock_guard guard(s.mutex);
    s.valid = true;
    s.sample = sample;
    s.counters = s.local;
  }
  if (!line.empty()) {
    MCLA_APP_INFO("{}", line);
  }
}

// Warp start: r3 = the manager, f1 = the target hour, f2 = the rate.
REX_EXTERN(sub_822F1518) {
  auto &s = State();
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  s.warp_lr.store(lr, std::memory_order_relaxed);
  s.warp_calls.fetch_add(1, std::memory_order_relaxed);
  if (tod::IsPresetWarp(lr)) {
    s.preset_warps.fetch_add(1, std::memory_order_relaxed);
    // Release: the update hook's acquire sees the marker.
    s.arcade_seen.store(true, std::memory_order_release);
    const int minutes = StartMinutes();
    if (tod::ReplaceWarpTarget(lr, minutes)) {
      const double from = ctx.f1.f64;
      ctx.f1.f64 = double(tod::StartHour(minutes));
      s.start_replaced.fetch_add(1, std::memory_order_relaxed);
      MCLA_APP_INFO("MCLA_TOD_CTL v=1 kind=start from={:06.3f} to={:06.3f} start={} "
                    "lr=0x{:08X}",
                    from, ctx.f1.f64, tod::StartLabel(minutes), lr);
    }
  }
  __imp__sub_822F1518(ctx, base);
}

std::string MclaTodStateJson() {
  auto &s = State();
  tod::Counters counters;
  tod::Sample sample;
  bool valid = false;
  {
    std::lock_guard guard(s.mutex);
    counters = s.counters;
    sample = s.sample;
    valid = s.valid;
  }
  counters.warp_calls = s.warp_calls.load(std::memory_order_relaxed);
  counters.preset_warps = s.preset_warps.load(std::memory_order_relaxed);
  counters.start_replaced = s.start_replaced.load(std::memory_order_relaxed);
  return tod::StateJson(valid, sample, tod::RateLabel(RateFactor()),
                        tod::StartLabel(StartMinutes()),
                        s.arcade_seen.load(std::memory_order_relaxed),
                        s.warp_lr.load(std::memory_order_relaxed), counters);
}
