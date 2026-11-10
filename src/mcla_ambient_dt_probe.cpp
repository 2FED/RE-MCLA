// KI-054 ambient-traffic frame delta (MCLA_AMBDT v=2). See
// mcla_ambient_dt_probe.h for the guest layout and its citations, and
// docs/evidence/M11-ki054-l83-implementation-20261110.md (v1) and
// docs/evidence/M11-l84-owner-build-20261110.md (v2).
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
//     (before the worker signals the join); v2: also the inline call (return
//     0x822D3D20, the path retail takes): samples the main timer +8/+80/+73/
//     +76 and the manager byte mgr+0x64259 at entry, attributes the site
//     reads made during the step and writes one MCLA_AMBDT line per 300
//     inline steps (probe only);
//   sub_82387A18 (predicate): site A, f30 as loaded at 0x826D796C;
//   sub_826FBA60 (suspension update): site B, f1 as loaded at 0x826FA300;
//   sub_826FA8A8 (pose countdown): site C, +8 sampled at entry.
// Every site read also records the split flag +73 at that moment and, when
// set, compares the seen delta with the saved full delta +80 (any thread).
// MclaAmbientDtOnTimerUpdate (called by the raw timer after each main-timer
// tick) counts the inline steps per timer update.
// Arm 1 replaces f30 (site A, after the predicate returns; f30 is
// non-volatile) and f1 (site B, before the call) with the kick-time delta
// when the worker's loaded value differs from it. It is served only inside a
// kick/join window, which retail never opens (the worker is not started
// without the command-line parameter behind mgr+0x64259), so Arm 1 is inert
// in retail; the key stays so that arm files remain valid. Nothing else
// changes; guest memory is never written.

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
    "Log the frame delta the ambient-traffic step reads (inline and worker "
    "paths) against the timer (KI-054; MCLA_AMBDT lines; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_ambient_dt_snapshot, false, "MCLA",
    "Give the ambient-traffic thread the kick-time frame delta at its two "
    "register-visible reads (KI-054 H1 fix A/B; inert in retail: no worker)")
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
  // v2: site reads outside any worker or inline step with +73 set (and,
  // of those, the ones that differ from +80).
  std::array<std::atomic<uint64_t>, amb::kSiteCount> outside_split{};
  std::array<std::atomic<uint64_t>, amb::kSiteCount> outside_split_short{};
  std::atomic<int> threaded{-1};  // last seen mgr+0x64259
  std::atomic<bool> start_logged{false};
  std::atomic<bool> inline_start_logged{false};
  std::mutex mutex;
  amb::Stats interval;  // since the last line
  amb::Stats total;     // earlier intervals
  // v2 (under the mutex): the latest main-timer update and the inline steps
  // since it, and the previous inline step's delta.
  bool has_update = false;
  uint32_t timer_bits = 0;
  double applied = -1.0;
  uint64_t steps_since_update = 0;
  bool has_previous_step = false;
  float previous_step_dt = 0.0f;
};

Shared &State() {
  static Shared state;
  return state;
}

// Per thread: `active` only on the worker inside sub_826D75D8 called from the
// worker loop while a window is open; `inline_active` inside the inline call.
struct Worker {
  bool active = false;
  bool inline_active = false;
  bool site_a_timer = false;
  uint64_t window = 0;
  float kick = 0.0f;
  amb::Stats stats;
};
thread_local Worker worker;

void Outside(uint8_t site) {
  State().outside[site].fetch_add(1, std::memory_order_relaxed);
}

// v2: the split state at a site read (any thread). Reads in a worker or an
// inline step go to that step's counters, the rest to atomics.
void SplitAtRead(uint8_t *base, uint8_t site, float seen) {
  const bool split = REX_LOAD_U8(amb::kMainTimer + amb::kSplitFlagOffset) != 0;
  const float full = split ? AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kSavedDeltaOffset))
                           : 0.0f;
  if (worker.active || worker.inline_active) {
    worker.stats.inl.AddSiteRead(site, split, full, seen);
    if (worker.inline_active) {
      ++worker.stats.inl.inline_reads[site];
    }
    return;
  }
  if (split) {
    auto &s = State();
    s.outside_split[site].fetch_add(1, std::memory_order_relaxed);
    if (amb::Differs(full, seen)) {
      s.outside_split_short[site].fetch_add(1, std::memory_order_relaxed);
    }
  }
}

// Moves the counters kept in atomics into `stats` (caller holds the mutex).
void DrainAtomics(Shared &s, amb::Stats &stats) {
  for (size_t i = 0; i < amb::kSiteCount; ++i) {
    stats.outside[i] += s.outside[i].exchange(0, std::memory_order_relaxed);
    stats.inl.split_any[i] += s.outside_split[i].exchange(0, std::memory_order_relaxed);
    stats.inl.split_short[i] +=
        s.outside_split_short[i].exchange(0, std::memory_order_relaxed);
  }
  stats.racer_outside += s.racer_outside.exchange(0, std::memory_order_relaxed);
}

// Closes the interval: returns the line (probe only) and moves the interval
// into the totals (caller holds the mutex).
std::string Flush(Shared &s) {
  auto &in = s.interval;
  DrainAtomics(s, in);
  std::string line;
  if (ProbeOn()) {
    line = amb::Line(in, s.window.load(std::memory_order_relaxed),
                     AsFloat(s.kick_bits.load(std::memory_order_relaxed)), ProbeOn(),
                     SnapshotOn(), s.threaded.load(std::memory_order_relaxed));
  }
  s.total.Merge(in);
  in = amb::Stats{};
  return line;
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
      MCLA_APP_INFO("MCLA_AMBDT v=2 kind=start path=worker probe={} snapshot={} kick_dt={:.6f} "
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
      line = Flush(s);
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

// v2: the inline (non-threaded) ambient step, called from sub_822D3C20 on
// the main thread; r3 = the ambient manager.
static void InlineStep(PPCContext &ctx, uint8_t *base) {
  auto &s = State();
  amb::InlineSample sample;
  sample.dt = AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kDeltaOffset));
  sample.full = AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kSavedDeltaOffset));
  sample.split = REX_LOAD_U8(amb::kMainTimer + amb::kSplitFlagOffset);
  sample.n = REX_LOAD_U32(amb::kMainTimer + amb::kSplitCountOffset);
  const uint32_t manager = ctx.r3.u32;
  if (manager != 0) {
    // The byte the game loop's kick guard reads (`lbzx r10,r11,r21`).
    s.threaded.store(REX_LOAD_U8(manager + amb::kThreadedFlagOffset) != 0 ? 1 : 0,
                     std::memory_order_relaxed);
  }
  if (ProbeOn() && !s.inline_start_logged.exchange(true)) {
    MCLA_APP_INFO("MCLA_AMBDT v=2 kind=start path=inline probe={} snapshot={} threaded={} "
                  "dt={:.6f} full={:.6f} split={} n={} scale={:.3f} unscaled={:.6f}",
                  ProbeOn() ? 1 : 0, SnapshotOn() ? 1 : 0,
                  s.threaded.load(std::memory_order_relaxed), double(sample.dt),
                  double(sample.full), unsigned(sample.split), sample.n,
                  double(AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kScaleOffset))),
                  double(AsFloat(REX_LOAD_U32(amb::kMainTimer + amb::kUnscaledOffset))));
  }
  const bool outer_inline = worker.inline_active;
  worker.inline_active = true;
  worker.site_a_timer = false;
  worker.stats = amb::Stats{};
  __imp__sub_826D75D8(ctx, base);
  worker.inline_active = outer_inline;
  std::string line;
  {
    std::lock_guard guard(s.mutex);
    sample.timer_dt = AsFloat(s.timer_bits);
    sample.applied = s.applied;
    worker.stats.inl.AddStep(sample, s.has_previous_step, s.previous_step_dt, s.has_update);
    s.has_previous_step = true;
    s.previous_step_dt = sample.dt;
    ++s.steps_since_update;
    s.interval.Merge(worker.stats);
    if (s.interval.inl.inline_steps >= amb::kLogInlineSteps) {
      line = Flush(s);
    }
  }
  worker.stats = amb::Stats{};
  if (!line.empty()) {
    MCLA_APP_INFO("{}", line);
  }
}

// Ambient step: r3 = the ambient manager.
REX_EXTERN(sub_826D75D8) {
  if (!AnyOn()) {
    __imp__sub_826D75D8(ctx, base);
    return;
  }
  auto &s = State();
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  if (amb::IsInlineStep(lr)) {
    InlineStep(ctx, base);
    return;
  }
  if (!amb::IsWorkerStep(lr, s.open.load(std::memory_order_acquire))) {
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
    SplitAtRead(base, amb::kSiteA, seen);
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
    SplitAtRead(base, amb::kSiteB, seen);
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
      SplitAtRead(base, amb::kSiteC, seen);
      if (worker.active) {
        worker.stats.AddRead(amb::kSiteC, worker.window, worker.kick, seen);
      } else {
        Outside(amb::kSiteC);
      }
    }
  }
  __imp__sub_826FA8A8(ctx, base);
}

void MclaAmbientDtOnTimerUpdate(uint8_t *base, double applied) {
  if (!AnyOn()) {
    return;
  }
  auto &s = State();
  const uint32_t bits = REX_LOAD_U32(amb::kMainTimer + amb::kDeltaOffset);
  std::lock_guard guard(s.mutex);
  if (s.has_update) {
    s.interval.inl.AddUpdate(s.steps_since_update);
  }
  s.has_update = true;
  s.steps_since_update = 0;
  s.timer_bits = bits;
  s.applied = applied;
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
                        SnapshotOn(), s.threaded.load(std::memory_order_relaxed));
}
