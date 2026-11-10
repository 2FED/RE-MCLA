#pragma once

// KI-054 ambient-traffic frame delta (MCLA_AMBDT v=2): pure parts of the
// default-off probe (mcla_ambient_dt_probe, Arm 0) and the default-off
// kick-time delta (mcla_ambient_dt_snapshot, Arm 1) of
// docs/evidence/M11-ki054-traffic-jitter-design-20261110.md (hypothesis H1).
// The hooks are in mcla_ambient_dt_probe.cpp; everything here is unit-tested
// (tests/mcla_ambient_dt_probe_test.cpp, which also checks the addresses,
// return addresses and registers against the generated code).
// l84: v2 measures the inline (main-thread) ambient step, the path retail
// takes; Arm 1 is inert in retail (see "v2" below).
//
// Static analysis (generated/default at the M11 codegen):
// - The game loop update sub_822C1FA8 restores the full frame delta to the
//   main timer (0x827D7500 +8 = f28) after the physics substeps and then
//   kicks the "[MC] Ambient Traffic" worker: `bl 0x826d9578` at 0x822C26B8
//   (return 0x822C26BC). sub_826D9578 signals the worker's slot-1 event (+16)
//   and wake event (+0). It joins later: `bl 0x826d95b0` at 0x822C2D04
//   (return 0x822C2D08); sub_826D95B0 waits on the slot-1 done event (+20).
// - The worker loop sub_826D9448 waits on the wake event and dispatches slot
//   1 to the ambient step sub_826D75D8 (`bl 0x826d75d8`, return 0x826D9500),
//   then signals the done event. sub_826D75D8 is also called directly by the
//   non-threaded path sub_822D3C20 (return 0x822D3D20); only the worker call
//   (return 0x826D9500) runs between kick and join.
// - The racer splitter sub_822A7E28 (racer +656 == 3 and +48 != 0) calls
//   sub_821BD910 on the main timer N times: with r4 = 1 it saves the full
//   delta in +80, sets +73 = 1, +76 = N and +8 = dt / N; the last pass
//   (r4 = 0) restores +8 from +80 and clears +73. While a racer is split, an
//   ambient read of +8 sees dt / N.
// - Site A, 0x826D796C in sub_826D75D8: `lfs f30,8(r30)` (r30 = 0x827D7500),
//   taken when the predicate sub_82387A18 called at 0x826D7954 (return
//   0x826D7958) returned 0 (the other path, `min(+92, 0.05)`, is the network
//   step and is left alone). The next call is `bl 0x82387a18` at 0x826D7990
//   (return 0x826D7994); f30 is not written in between, so a hook of
//   sub_82387A18 that sees lr == 0x826D7994 sees the value just loaded. f30
//   is non-volatile, so the guest keeps using that register afterwards
//   (ambient clock mgr+0x64238, countdown mgr+0x64244).
// - Site B, 0x826FA300 in sub_826FA0F8 (mcAmbientCollider vfunc +56):
//   `lfs f1,8(r19)` (r19 = 0x827D7500), taken when r20 == 0 (otherwise
//   f1 = f31). f1 is passed unchanged to `bl 0x826fba60` at 0x826FA374
//   (return 0x826FA378), the per-wheel suspension update; sub_826FBA60 has
//   no other caller. A hook of sub_826FBA60 with lr == 0x826FA378 and
//   r20 == 0 sees the value just loaded in f1.
// - Site C, 0x826FA8E0 in sub_826FA8A8 (mcAmbientPhysInst vfunc +108):
//   `lfs f0,8(r9)` a few instructions after entry, only when obj +140 has
//   bit 0x40; obj +132 -= dt (0.5 s pose re-validation countdown). There is
//   no call between the load and its use, so the probe samples +8 at the
//   function entry (an approximation of the loaded value) and Arm 1 does not
//   serve this site.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace mcla::ambient_dt {

// Main game timer and its fields.
inline constexpr uint32_t kMainTimer = 0x827D7500u;
inline constexpr uint32_t kDeltaOffset = 8;        // scaled frame delta (0x827D7508)
inline constexpr uint32_t kRateOffset = 12;        // 1 / delta
inline constexpr uint32_t kSplitFlagOffset = 73;   // byte: split in progress
inline constexpr uint32_t kSplitCountOffset = 76;  // N of the split
inline constexpr uint32_t kSavedDeltaOffset = 80;  // full delta while split
inline constexpr uint32_t kScaleOffset = 84;       // time scale
inline constexpr uint32_t kUnscaledOffset = 88;    // unscaled delta

// Kick / join of the ambient worker in the game loop update.
inline constexpr uint32_t kKick = 0x826D9578u;
inline constexpr uint32_t kKickSite = 0x822C26B8u;
inline constexpr uint32_t kKickReturn = 0x822C26BCu;
inline constexpr uint32_t kJoin = 0x826D95B0u;
inline constexpr uint32_t kJoinSite = 0x822C2D04u;
inline constexpr uint32_t kJoinReturn = 0x822C2D08u;
// The ambient step and its worker-loop return address.
inline constexpr uint32_t kAmbientStep = 0x826D75D8u;
inline constexpr uint32_t kWorkerReturn = 0x826D9500u;
inline constexpr uint32_t kInlineReturn = 0x822D3D20u;
// The racer splitter and its entry condition.
inline constexpr uint32_t kRacerSplitter = 0x822A7E28u;
inline constexpr uint32_t kRacerStateOffset = 656;
inline constexpr uint32_t kRacerSplitState = 3;
inline constexpr uint32_t kRacerTuneOffset = 48;
// Site A: the predicate call before the load and the one after it.
inline constexpr uint32_t kPredicate = 0x82387A18u;
inline constexpr uint32_t kSiteALoad = 0x826D796Cu;
inline constexpr uint32_t kSiteAGateReturn = 0x826D7958u;
inline constexpr uint32_t kSiteAReturn = 0x826D7994u;
// Site B: the suspension update called with the loaded delta in f1.
inline constexpr uint32_t kSuspension = 0x826FBA60u;
inline constexpr uint32_t kSiteBLoad = 0x826FA300u;
inline constexpr uint32_t kSiteBReturn = 0x826FA378u;
// Site C: the pose re-validation countdown.
inline constexpr uint32_t kPoseCountdown = 0x826FA8A8u;
inline constexpr uint32_t kSiteCLoad = 0x826FA8E0u;
inline constexpr uint32_t kPoseFlagsOffset = 140;
inline constexpr uint32_t kPoseFlagBit = 0x40;

// One MCLA_AMBDT line per kLogWindows completed kick/join windows.
inline constexpr uint64_t kLogWindows = 300;
// Two deltas differ when they are more than this apart (seconds).
inline constexpr double kTolerance = 1.0e-6;

enum Site : uint8_t { kSiteA = 0, kSiteB = 1, kSiteC = 2, kSiteCount = 3 };

inline char SiteName(uint8_t site) {
  return site == kSiteA ? 'a' : site == kSiteB ? 'b' : site == kSiteC ? 'c' : '?';
}

inline bool Differs(float kick, float seen) {
  return std::fabs(double(seen) - double(kick)) > kTolerance;
}

// A read is the ambient worker's when it happens inside sub_826D75D8 called
// from the worker loop while a kick/join window is open.
inline bool IsWorkerStep(uint32_t lr, bool window_open) {
  return window_open && lr == kWorkerReturn;
}

// Site A is the timer read when the gate predicate returned 0.
inline bool SiteAFromTimer(uint32_t gate_r3) {
  return (gate_r3 & 0xFFu) == 0;
}

// Site B is the timer read on the r20 == 0 path.
inline bool SiteBFromTimer(uint32_t r20) {
  return r20 == 0;
}

// Site C loads the delta only with the pose flag bit set.
inline bool SiteCReads(uint8_t flags) {
  return (flags & kPoseFlagBit) != 0;
}

// The racer splitter splits the timer for this racer.
inline bool RacerSplits(uint32_t state, uint32_t tune) {
  return state == kRacerSplitState && tune != 0;
}

// Arm 1 replaces a loaded value only on the worker, inside the window, at
// sites A and B, and only when it differs from the kick-time delta.
inline bool ShouldServe(bool snapshot_on, bool worker, uint8_t site, float kick, float seen) {
  return snapshot_on && worker && (site == kSiteA || site == kSiteB) && Differs(kick, seen);
}

// --- v2: the inline (non-threaded) ambient step --------------------------------
//
// The l83 PC run (owner, 2026-11-10) showed no kick/join window at all
// (windows=0, outside=[17887,26164,0]): the kick at 0x822C26B8 is guarded by
// the ambient manager (0x8286D8E4, `lwz r11,-10012(r22)`) and its byte
// mgr+0x64259 (`ori r21,r11,16985`, `lbzx r10,r11,r21`). The manager
// constructor sub_826D8E70 stores that byte as (word +4 of 0x8288E8F4 != 0)
// (`lwz r9,4(r4)`, `cntlzw`, `rlwinm ..,27,31,31`, `xori r8,r4,1`,
// `stbx r8,r31,r15`); 0x8288E8F4 is a command-line parameter object
// registered by the static initialiser sub_827AE6B0 through sub_821C06C8,
// which zeroes +4 (`stw r9,4(r3)` with r9 = 0). Without that parameter (the
// retail command line) the "[MC] Ambient Traffic" worker is never kicked and
// the ambient step runs inline on the main thread from sub_822D3C20 (return
// 0x822D3D20). Hypothesis H1 as written (the worker reads a split delta) is
// therefore refuted for retail, and Arm 1 (mcla_ambient_dt_snapshot, served
// only inside a kick/join window) is inert in retail; its key stays valid.
//
// Probe v2 measures the inline path: per inline step it samples the main
// timer +8 (seen delta), +80 (saved full delta), +73 (split flag) and +76
// (split count) at entry; every site A/B/C read records whether the split
// flag is set at that moment and compares the seen delta with +80. The raw
// timer (mcla_raw_timer.cpp) reports each main-timer update after the game's
// own tick has run (+8 as applied by the game, and the step the raw timer
// supplied), so the probe counts inline steps per timer update (0 / 1 / 2+)
// and inline steps whose delta differs from the latest timer update's or
// jumps by more than 20 % against the previous inline step.

inline constexpr uint32_t kAmbientManager = 0x8286D8E4u;  // manager pointer
inline constexpr uint32_t kThreadedFlagOffset = 0x64259;  // byte: worker on
inline constexpr uint32_t kThreadedParam = 0x8288E8F4u;   // command-line param
inline constexpr uint32_t kManagerCtor = 0x826D8E70u;
inline constexpr uint32_t kInlineCaller = 0x822D3C20u;
// One v2 line per kLogInlineSteps inline steps (or kLogWindows windows).
inline constexpr uint64_t kLogInlineSteps = 300;
// An inline step delta "jumps" when it is more than this fraction away from
// the previous inline step's delta.
inline constexpr double kJumpFraction = 0.20;

inline bool IsInlineStep(uint32_t lr) {
  return lr == kInlineReturn;
}

// Histogram bucket of inline steps between two timer updates: 0, 1, 2+.
inline size_t StepsBucket(uint64_t steps) {
  return steps >= 2 ? 2 : static_cast<size_t>(steps);
}

inline bool Jumped(float previous, float now) {
  return previous > 0.0f &&
         std::fabs(double(now) - double(previous)) > kJumpFraction * double(previous);
}

// max(now / previous, previous / now), 1 when either is not positive.
inline double StepRatio(float previous, float now) {
  if (!(previous > 0.0f) || !(now > 0.0f)) {
    return 1.0;
  }
  const double r = double(now) / double(previous);
  return r >= 1.0 ? r : 1.0 / r;
}

// The main timer at the entry of an inline step (and the latest update).
struct InlineSample {
  float dt = 0.0f;        // +8
  float full = 0.0f;      // +80 (meaningful while split)
  uint8_t split = 0;      // +73
  uint32_t n = 0;         // +76
  float timer_dt = 0.0f;  // +8 after the latest main-timer update
  double applied = -1.0;  // step supplied by the raw timer (-1: none)
};

struct InlineStats {
  uint64_t inline_steps = 0;    // sub_826D75D8 called from 0x822D3D20
  uint64_t timer_updates = 0;   // main-timer updates (after the first)
  std::array<uint64_t, 3> steps_per_update{};  // inline steps per update
  uint64_t split_at_step = 0;   // inline steps entered with +73 set
  uint64_t step_jumps = 0;      // > 20 % against the previous inline step
  uint64_t timer_mismatch = 0;  // dt differs from the latest update's +8
  double max_step_ratio = 1.0;  // largest StepRatio between inline steps
  float min_dt = 0.0f;
  float max_dt = 0.0f;
  std::array<uint64_t, kSiteCount> inline_reads{};  // site reads in an inline step
  std::array<uint64_t, kSiteCount> split_any{};     // site reads with +73 set
  std::array<uint64_t, kSiteCount> split_short{};   // ... seen differs from +80
  double max_split_ratio = 1.0;  // largest +80 / seen of a split read
  InlineSample last;
  bool has_last = false;

  // One site read on any thread; `split` is +73 at that moment.
  void AddSiteRead(uint8_t site, bool split, float full, float seen) {
    if (site >= kSiteCount || !split) {
      return;
    }
    ++split_any[site];
    if (!Differs(full, seen)) {
      return;
    }
    ++split_short[site];
    if (seen > 0.0f && full > 0.0f) {
      max_split_ratio = std::max(max_split_ratio, double(full) / double(seen));
    }
  }

  // One inline step; `previous` is the previous inline step's dt (if any),
  // `has_timer` whether a main-timer update has been seen.
  void AddStep(const InlineSample &s, bool has_previous, float previous, bool has_timer) {
    if (inline_steps == 0 || s.dt < min_dt) {
      min_dt = s.dt;
    }
    if (inline_steps == 0 || s.dt > max_dt) {
      max_dt = s.dt;
    }
    ++inline_steps;
    split_at_step += s.split != 0 ? 1 : 0;
    if (has_previous) {
      step_jumps += Jumped(previous, s.dt) ? 1 : 0;
      max_step_ratio = std::max(max_step_ratio, StepRatio(previous, s.dt));
    }
    if (has_timer && Differs(s.timer_dt, s.dt)) {
      ++timer_mismatch;
    }
    last = s;
    has_last = true;
  }

  // One main-timer update; `steps` inline steps ran since the previous one.
  void AddUpdate(uint64_t steps) {
    ++timer_updates;
    ++steps_per_update[StepsBucket(steps)];
  }

  void Merge(const InlineStats &o) {
    if (o.inline_steps != 0) {
      min_dt = inline_steps == 0 ? o.min_dt : std::min(min_dt, o.min_dt);
      max_dt = inline_steps == 0 ? o.max_dt : std::max(max_dt, o.max_dt);
    }
    inline_steps += o.inline_steps;
    timer_updates += o.timer_updates;
    split_at_step += o.split_at_step;
    step_jumps += o.step_jumps;
    timer_mismatch += o.timer_mismatch;
    max_step_ratio = std::max(max_step_ratio, o.max_step_ratio);
    max_split_ratio = std::max(max_split_ratio, o.max_split_ratio);
    for (size_t i = 0; i < 3; ++i) {
      steps_per_update[i] += o.steps_per_update[i];
    }
    for (size_t i = 0; i < kSiteCount; ++i) {
      inline_reads[i] += o.inline_reads[i];
      split_any[i] += o.split_any[i];
      split_short[i] += o.split_short[i];
    }
    if (o.has_last) {
      last = o.last;
      has_last = true;
    }
  }
};

inline std::string SampleText(const InlineStats &v) {
  if (!v.has_last) {
    return "[]";
  }
  char text[160];
  std::snprintf(text, sizeof(text), "[%.6f:%.6f:%u:%u:%.6f:%.6f]", double(v.last.dt),
                double(v.last.full), unsigned(v.last.split), unsigned(v.last.n),
                double(v.last.timer_dt), v.last.applied);
  return text;
}

struct Mismatch {
  uint64_t window = 0;
  uint8_t site = 0;
  float kick = 0.0f;
  float seen = 0.0f;
};

struct Stats {
  uint64_t windows = 0;        // completed kick/join windows
  uint64_t overlaps = 0;       // windows with at least one racer split
  uint64_t racer_splits = 0;   // racer splitter calls that split, inside a window
  uint64_t racer_outside = 0;  // racer splitter calls that split, no window open
  uint64_t worker_steps = 0;   // worker calls of the ambient step inside a window
  uint64_t join_changed = 0;   // +8 at the join differs from the kick value
  uint64_t kick_split = 0;     // kicks with the split flag (+73) already set
  std::array<uint64_t, kSiteCount> reads{};        // worker reads inside a window
  std::array<uint64_t, kSiteCount> split_reads{};  // ... that differ from the kick value
  std::array<uint64_t, kSiteCount> outside{};      // reads not by the worker step
  std::array<uint64_t, 2> served{};                // Arm 1 replacements (A, B)
  double max_abs_delta = 0.0;
  double min_ratio = 1.0;
  bool has_ratio = false;
  std::array<Mismatch, 3> last{};  // most recent first
  uint32_t last_count = 0;
  InlineStats inl;  // v2: the inline path and the timer updates

  void AddRead(uint8_t site, uint64_t window, float kick, float seen) {
    if (site >= kSiteCount) {
      return;
    }
    ++reads[site];
    if (!Differs(kick, seen)) {
      return;
    }
    ++split_reads[site];
    const double delta = std::fabs(double(seen) - double(kick));
    if (delta > max_abs_delta) {
      max_abs_delta = delta;
    }
    if (kick > 0.0f) {
      const double ratio = double(seen) / double(kick);
      if (!has_ratio || ratio < min_ratio) {
        min_ratio = ratio;
      }
      has_ratio = true;
    }
    PushMismatch({window, site, kick, seen});
  }

  void PushMismatch(const Mismatch &m) {
    for (size_t i = last.size() - 1; i > 0; --i) {
      last[i] = last[i - 1];
    }
    last[0] = m;
    if (last_count < last.size()) {
      ++last_count;
    }
  }

  // Adds the counters of `other`; its mismatches are newer than ours.
  void Merge(const Stats &other) {
    windows += other.windows;
    overlaps += other.overlaps;
    racer_splits += other.racer_splits;
    racer_outside += other.racer_outside;
    worker_steps += other.worker_steps;
    join_changed += other.join_changed;
    kick_split += other.kick_split;
    for (size_t i = 0; i < kSiteCount; ++i) {
      reads[i] += other.reads[i];
      split_reads[i] += other.split_reads[i];
      outside[i] += other.outside[i];
    }
    served[0] += other.served[0];
    served[1] += other.served[1];
    if (other.max_abs_delta > max_abs_delta) {
      max_abs_delta = other.max_abs_delta;
    }
    if (other.has_ratio && (!has_ratio || other.min_ratio < min_ratio)) {
      min_ratio = other.min_ratio;
      has_ratio = true;
    }
    for (uint32_t i = other.last_count; i > 0; --i) {
      PushMismatch(other.last[i - 1]);
    }
    inl.Merge(other.inl);
  }
};

inline std::string Triple(const std::array<uint64_t, kSiteCount> &v) {
  char text[96];
  std::snprintf(text, sizeof(text), "%llu/%llu/%llu", static_cast<unsigned long long>(v[0]),
                static_cast<unsigned long long>(v[1]), static_cast<unsigned long long>(v[2]));
  return text;
}

inline std::string MismatchList(const Stats &s) {
  std::string text = "[";
  for (uint32_t i = 0; i < s.last_count && i < s.last.size(); ++i) {
    char item[96];
    std::snprintf(item, sizeof(item), "%s%llu:%c:%.6f:%.6f", i ? "," : "",
                  static_cast<unsigned long long>(s.last[i].window), SiteName(s.last[i].site),
                  double(s.last[i].kick), double(s.last[i].seen));
    text += item;
  }
  return text + "]";
}

// The periodic v2 line over the last interval: the inline-path counters
// first, then the v1 kick/join window counters. `window` is the id of the
// last kick/join window, `kick_dt` its kick-time delta, `threaded` the last
// seen manager byte mgr+0x64259 (-1 unknown).
inline std::string Line(const Stats &s, uint64_t window, float kick_dt, bool probe,
                        bool snapshot, int threaded) {
  const InlineStats &v = s.inl;
  const uint64_t split_total = s.split_reads[0] + s.split_reads[1] + s.split_reads[2];
  char head[640];
  std::snprintf(head, sizeof(head),
                "MCLA_AMBDT v=2 probe=%d snapshot=%d threaded=%d inline_steps=%llu "
                "timer_updates=%llu steps_per_update=%s split_at_step=%llu step_jumps=%llu "
                "timer_mismatch=%llu step_dt=%.6f/%.6f max_step_ratio=%.4f inline_reads=%s "
                "split_any=%s split_short=%s max_split_ratio=%.4f last_step=%s ",
                probe ? 1 : 0, snapshot ? 1 : 0, threaded,
                static_cast<unsigned long long>(v.inline_steps),
                static_cast<unsigned long long>(v.timer_updates),
                Triple(v.steps_per_update).c_str(),
                static_cast<unsigned long long>(v.split_at_step),
                static_cast<unsigned long long>(v.step_jumps),
                static_cast<unsigned long long>(v.timer_mismatch), double(v.min_dt),
                double(v.max_dt), v.max_step_ratio, Triple(v.inline_reads).c_str(),
                Triple(v.split_any).c_str(), Triple(v.split_short).c_str(), v.max_split_ratio,
                SampleText(v).c_str());
  char tail[640];
  std::snprintf(tail, sizeof(tail),
                "window=%llu windows=%llu overlaps=%llu racer_splits=%llu racer_outside=%llu "
                "worker_steps=%llu split_reads=%llu reads=%s split=%s outside=%s "
                "served=%llu/%llu max_abs_dt_delta=%.6f min_ratio=%.4f kick_dt=%.6f "
                "join_changed=%llu kick_split=%llu last=",
                static_cast<unsigned long long>(window),
                static_cast<unsigned long long>(s.windows),
                static_cast<unsigned long long>(s.overlaps),
                static_cast<unsigned long long>(s.racer_splits),
                static_cast<unsigned long long>(s.racer_outside),
                static_cast<unsigned long long>(s.worker_steps),
                static_cast<unsigned long long>(split_total), Triple(s.reads).c_str(),
                Triple(s.split_reads).c_str(), Triple(s.outside).c_str(),
                static_cast<unsigned long long>(s.served[0]),
                static_cast<unsigned long long>(s.served[1]), s.max_abs_delta,
                s.has_ratio ? s.min_ratio : 1.0, double(kick_dt),
                static_cast<unsigned long long>(s.join_changed),
                static_cast<unsigned long long>(s.kick_split));
  return std::string(head) + tail + MismatchList(s);
}

inline std::string JsonTriple(const std::array<uint64_t, kSiteCount> &v) {
  char text[96];
  std::snprintf(text, sizeof(text), "[%llu, %llu, %llu]", static_cast<unsigned long long>(v[0]),
                static_cast<unsigned long long>(v[1]), static_cast<unsigned long long>(v[2]));
  return text;
}

// The F10 "ambient_dt" object (session totals).
inline std::string StateJson(const Stats &s, uint64_t window, float kick_dt, bool probe,
                             bool snapshot, int threaded) {
  const InlineStats &v = s.inl;
  char head[768];
  std::snprintf(head, sizeof(head),
                "{\"version\": 2, \"probe\": %s, \"snapshot\": %s, \"threaded\": %d, "
                "\"inline_steps\": %llu, \"timer_updates\": %llu, \"steps_per_update\": %s, "
                "\"split_at_step\": %llu, \"step_jumps\": %llu, \"timer_mismatch\": %llu, "
                "\"step_dt_min\": %.6f, \"step_dt_max\": %.6f, \"max_step_ratio\": %.4f, "
                "\"inline_reads\": %s, \"split_any\": %s, \"split_short\": %s, "
                "\"max_split_ratio\": %.4f, ",
                probe ? "true" : "false", snapshot ? "true" : "false", threaded,
                static_cast<unsigned long long>(v.inline_steps),
                static_cast<unsigned long long>(v.timer_updates),
                JsonTriple(v.steps_per_update).c_str(),
                static_cast<unsigned long long>(v.split_at_step),
                static_cast<unsigned long long>(v.step_jumps),
                static_cast<unsigned long long>(v.timer_mismatch), double(v.min_dt),
                double(v.max_dt), v.max_step_ratio, JsonTriple(v.inline_reads).c_str(),
                JsonTriple(v.split_any).c_str(), JsonTriple(v.split_short).c_str(),
                v.max_split_ratio);
  char tail[768];
  std::snprintf(tail, sizeof(tail),
                "\"window\": %llu, \"kick_dt\": %.6f, \"windows\": %llu, \"overlaps\": %llu, "
                "\"racer_splits\": %llu, \"racer_outside\": %llu, \"worker_steps\": %llu, "
                "\"reads\": %s, \"split_reads\": %s, \"outside\": %s, \"served\": [%llu, %llu], "
                "\"max_abs_dt_delta\": %.6f, \"min_ratio\": %.4f, \"join_changed\": %llu, "
                "\"kick_split\": %llu}",
                static_cast<unsigned long long>(window), double(kick_dt),
                static_cast<unsigned long long>(s.windows),
                static_cast<unsigned long long>(s.overlaps),
                static_cast<unsigned long long>(s.racer_splits),
                static_cast<unsigned long long>(s.racer_outside),
                static_cast<unsigned long long>(s.worker_steps), JsonTriple(s.reads).c_str(),
                JsonTriple(s.split_reads).c_str(), JsonTriple(s.outside).c_str(),
                static_cast<unsigned long long>(s.served[0]),
                static_cast<unsigned long long>(s.served[1]), s.max_abs_delta,
                s.has_ratio ? s.min_ratio : 1.0, static_cast<unsigned long long>(s.join_changed),
                static_cast<unsigned long long>(s.kick_split));
  return std::string(head) + tail;
}

}  // namespace mcla::ambient_dt

// The F10 state block "ambient_dt"; empty when both switches are off.
std::string MclaAmbientDtStateJson();

// Called by the raw timer (mcla_raw_timer.cpp) after each main-timer tick
// (sub_821BDA90 on 0x827D7500) has run; `applied` is the step the raw timer
// supplied in f1, or -1. A no-op unless the probe or the snapshot is on.
void MclaAmbientDtOnTimerUpdate(uint8_t *base, double applied);
