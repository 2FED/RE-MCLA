// KI-054 ambient-traffic frame delta (MCLA_AMBDT v=1). See
// mcla_ambient_dt_probe.h for the guest layout and its citations, and
// docs/evidence/M11-ki054-l83-implementation-20261110.md.
//
// Seven strong overrides, each active only with one of the init-only switches
// mcla_ambient_dt_probe (Arm 0, read-only) or mcla_ambient_dt_snapshot
// (Arm 1); both default off, and with both off every hook is one static bool
// test and the original call. Each calls the ORIGINAL; only guest registers
// are read, plus guest words the guest itself reads at the same point:
//   sub_826D9578 (kick, from 0x822C26B8): records the main timer +8 (the
//     kick-time delta) and opens the window before the original signals the
//     worker;
//   sub_826D95B0 (join, from 0x822C2D04): after the original has waited for
//     the worker, closes the window, counts it and writes one MCLA_AMBDT line
//     per 300 windows (probe only);
//   sub_822A7E28 (racer splitter): counts racers that split the timer, inside
//     and outside a window;
//   sub_826D75D8 (ambient step): marks the worker call (return 0x826D9500)
//     inside a window on this thread and merges its counts when it returns
//     (before the worker signals the join);
//   sub_82387A18 (predicate): site A, f30 as loaded at 0x826D796C;
//   sub_826FBA60 (suspension update): site B, f1 as loaded at 0x826FA300;
//   sub_826FA8A8 (pose countdown): site C, +8 sampled at entry.
// Arm 1 replaces f30 (site A, after the predicate returns; f30 is
// non-volatile) and f1 (site B, before the call) with the kick-time delta
// when the worker's loaded value differs from it. Nothing else changes; guest
// memory is never written.

#include "mcla_ambient_dt_probe.h"

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>

REXCVAR_DEFINE_BOOL(mcla_ambient_dt_probe, false, "MCLA",
    "Log the frame delta the ambient-traffic thread reads against the kick-time "
    "value (KI-054 H1; MCLA_AMBDT lines; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_ambient_dt_snapshot, false, "MCLA",
    "Give the ambient-traffic thread the kick-time frame delta at its two "
    "register-visible reads (KI-054 H1 fix A/B)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace amb = mcla::ambient_dt;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_ambient_dt_probe);
  return on;
}

bool SnapshotOn() {
  static const bool on = REXCVAR_GET(mcla_ambient_dt_snapshot);
  return on;
}

bool AnyOn() {
  static const bool on = ProbeOn() || SnapshotOn();
  return on;
}

float AsFloat(uint32_t bits) {
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

struct Shared {
  // Written by the main thread at the kick (before the worker is signalled)
  // and read by the worker inside the window.
  std::atomic<bool> open{false};
  std::atomic<uint32_t> kick_bits{0};
  std::atomic<uint64_t> window{0};
  std::atomic<uint32_t> kick_split{0};
  std::atomic<uint64_t> window_splits{0};
  std::atomic<uint64_t> racer_outside{0};
  std::array<std::atomic<uint64_t>, amb::kSiteCount> outside{};
  std::atomic<bool> start_logged{false};
  std::mutex mutex;
  amb::Stats interval;  // since the last line
  amb::Stats total;     // earlier intervals
};

Shared &State() {
  static Shared state;
  return state;
}

// Per thread: set only on the worker inside sub_826D75D8 called from the
// worker loop while a window is open.
struct Worker {
  bool active = false;
  bool site_a_timer = false;
  uint64_t window = 0;
  float kick = 0.0f;
  amb::Stats stats;
};
thread_local Worker worker;

void Outside(uint8_t site) {
  State().outside[site].fetch_add(1, std::memory_order_relaxed);
}

// Moves the counters kept in atomics into `stats` (caller holds the mutex).
void DrainAtomics(Shared &s, amb::Stats &stats) {
  for (size_t i = 0; i < amb::kSiteCount; ++i) {
    stats.outside[i] += s.outside[i].exchange(0, std::memory_order_relaxed);
  }
  stats.racer_outside += s.racer_outside.exchange(0, std::memory_order_relaxed);
}

}  // namespace

// Kick: r3 = the worker's event block; called only from 0x822C26B8.
REX_EXTERN(sub_826D9578) {
  if (AnyOn() && static_cast<uint32_t>(ctx.lr) == amb::kKickReturn) {
    auto &s = State();
    const uint32_t bits = REX_LOAD_U32(amb::kMainTimer + amb::kDeltaOffset);
    const bool split = REX_LOAD_U8(amb::kMainTimer + amb::kSplitFlagOffset) != 0;
    s.kick_bits.store(bits, std::memory_order_relaxed);
    s.kick_split.store(split ? 1u : 0u, std::memory_order_relaxed);
    s.window_splits.store(0, std::memory_order_relaxed);
    s.window.fetch_add(1, std::memory_order_relaxed);
    // Release: the worker's acquire of `open` sees the kick value.
    s.open.store(true, std::memory_order_release);
    if (ProbeOn() && !s.start_logged.exchange(true)) {
      MCLA_APP_INFO("MCLA_AMBDT v=1 kind=start probe={} snapshot={} kick_dt={:.6f} "
                    "rate={:.3f} scale={:.3f} unscaled={:.6f} split={}",
                    ProbeOn() ? 1 : 0, SnapshotOn() ? 1 : 0, double(AsFloat(bits)),
                    double(AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kRateOffset))),
                    double(AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kScaleOffset))),
                    double(AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kUnscaledOffset))),
                    split ? 1 : 0);
    }
  }
  __imp__sub_826D9578(ctx, base);
}

// Join: waits for the worker's slot-1 done event; called only from 0x822C2D04.
REX_EXTERN(sub_826D95B0) {
  const bool track = AnyOn() && static_cast<uint32_t>(ctx.lr) == amb::kJoinReturn &&
                     State().open.load(std::memory_order_acquire);
  __imp__sub_826D95B0(ctx, base);
  if (!track) {
    return;
  }
  auto &s = State();
  const uint32_t now_bits = REX_LOAD_U32(amb::kMainTimer + amb::kDeltaOffset);
  const uint32_t kick_bits = s.kick_bits.load(std::memory_order_relaxed);
  const uint64_t window = s.window.load(std::memory_order_relaxed);
  s.open.store(false, std::memory_order_release);
  const uint64_t splits = s.window_splits.exchange(0, std::memory_order_relaxed);
  std::string line;
  {
    std::lock_guard guard(s.mutex);
    auto &in = s.interval;
    ++in.windows;
    in.overlaps += splits != 0 ? 1 : 0;
    in.racer_splits += splits;
    in.join_changed += now_bits != kick_bits ? 1 : 0;
    in.kick_split += s.kick_split.load(std::memory_order_relaxed);
    if (in.windows >= amb::kLogWindows) {
      DrainAtomics(s, in);
      if (ProbeOn()) {
        line = amb::Line(in, window, AsFloat(kick_bits), ProbeOn(), SnapshotOn());
      }
      s.total.Merge(in);
      in = amb::Stats{};
    }
  }
  if (!line.empty()) {
    MCLA_APP_INFO("{}", line);
  }
}

// Racer splitter: r3 = racer.
REX_EXTERN(sub_822A7E28) {
  if (AnyOn()) {
    const uint32_t racer = ctx.r3.u32;
    if (racer != 0 && amb::RacerSplits(REX_LOAD_U32(racer + amb::kRacerStateOffset),
                                       REX_LOAD_U32(racer + amb::kRacerTuneOffset))) {
      auto &s = State();
      if (s.open.load(std::memory_order_acquire)) {
        s.window_splits.fetch_add(1, std::memory_order_relaxed);
      } else {
        s.racer_outside.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }
  __imp__sub_822A7E28(ctx, base);
}

// Ambient step: r3 = the ambient manager.
REX_EXTERN(sub_826D75D8) {
  if (!AnyOn()) {
    __imp__sub_826D75D8(ctx, base);
    return;
  }
  auto &s = State();
  if (!amb::IsWorkerStep(static_cast<uint32_t>(ctx.lr),
                         s.open.load(std::memory_order_acquire))) {
    __imp__sub_826D75D8(ctx, base);
    return;
  }
  worker.active = true;
  worker.site_a_timer = false;
  worker.window = s.window.load(std::memory_order_relaxed);
  worker.kick = AsFloat(s.kick_bits.load(std::memory_order_relaxed));
  worker.stats = amb::Stats{};
  worker.stats.worker_steps = 1;
  __imp__sub_826D75D8(ctx, base);
  worker.active = false;
  std::lock_guard guard(s.mutex);
  s.interval.Merge(worker.stats);
}

// Predicate used at many sites; only the two calls around site A are
// examined (by return address).
REX_EXTERN(sub_82387A18) {
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  if (!AnyOn() || (lr != amb::kSiteAGateReturn && lr != amb::kSiteAReturn)) {
    __imp__sub_82387A18(ctx, base);
    return;
  }
  if (lr == amb::kSiteAGateReturn) {
    __imp__sub_82387A18(ctx, base);
    // 0 selects the timer read at 0x826D796C; the next call (return
    // 0x826D7994) always follows on that path and consumes the flag.
    worker.site_a_timer = amb::SiteAFromTimer(ctx.r3.u32);
    return;
  }
  const bool from_timer = worker.site_a_timer;
  worker.site_a_timer = false;
  const float seen = static_cast<float>(ctx.f30.f64);
  bool serve = false;
  if (from_timer) {
    if (worker.active) {
      worker.stats.AddRead(amb::kSiteA, worker.window, worker.kick, seen);
      serve = amb::ShouldServe(SnapshotOn(), true, amb::kSiteA, worker.kick, seen);
    } else {
      Outside(amb::kSiteA);
    }
  }
  __imp__sub_82387A18(ctx, base);
  if (serve) {
    // As `lfs f30,8(r30)` would have loaded the kick-time value.
    ctx.f30.f64 = static_cast<double>(worker.kick);
    ++worker.stats.served[0];
  }
}

// Suspension update: f1 = the delta loaded at 0x826FA300 when called from
// 0x826FA374 with r20 == 0.
REX_EXTERN(sub_826FBA60) {
  if (AnyOn() && static_cast<uint32_t>(ctx.lr) == amb::kSiteBReturn &&
      amb::SiteBFromTimer(ctx.r20.u32)) {
    const float seen = static_cast<float>(ctx.f1.f64);
    if (worker.active) {
      worker.stats.AddRead(amb::kSiteB, worker.window, worker.kick, seen);
      if (amb::ShouldServe(SnapshotOn(), true, amb::kSiteB, worker.kick, seen)) {
        ctx.f1.f64 = static_cast<double>(worker.kick);
        ++worker.stats.served[1];
      }
    } else {
      Outside(amb::kSiteB);
    }
  }
  __imp__sub_826FBA60(ctx, base);
}

// Pose countdown: r3 = the physics instance; the delta is read a few
// instructions after entry when +140 has bit 0x40 (sampled here).
REX_EXTERN(sub_826FA8A8) {
  if (AnyOn()) {
    const uint32_t obj = ctx.r3.u32;
    if (obj != 0 && amb::SiteCReads(REX_LOAD_U8(obj + amb::kPoseFlagsOffset))) {
      const float seen = AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kDeltaOffset));
      if (worker.active) {
        worker.stats.AddRead(amb::kSiteC, worker.window, worker.kick, seen);
      } else {
        Outside(amb::kSiteC);
      }
    }
  }
  __imp__sub_826FA8A8(ctx, base);
}

std::string MclaAmbientDtStateJson() {
  if (!AnyOn()) {
    return {};
  }
  auto &s = State();
  std::lock_guard guard(s.mutex);
  DrainAtomics(s, s.interval);
  amb::Stats all = s.total;
  all.Merge(s.interval);
  return amb::StateJson(all, s.window.load(std::memory_order_relaxed),
                        AsFloat(s.kick_bits.load(std::memory_order_relaxed)), ProbeOn(),
                        SnapshotOn());
}
