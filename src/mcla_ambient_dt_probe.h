#pragma once

// KI-054 ambient-traffic frame delta (MCLA_AMBDT v=1): pure parts of the
// default-off probe (mcla_ambient_dt_probe, Arm 0) and the default-off
// kick-time delta (mcla_ambient_dt_snapshot, Arm 1) of
// docs/evidence/M11-ki054-traffic-jitter-design-20261110.md (hypothesis H1).
// The hooks are in mcla_ambient_dt_probe.cpp; everything here is unit-tested
// (tests/mcla_ambient_dt_probe_test.cpp, which also checks the addresses,
// return addresses and registers against the generated code).
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

// The periodic line over the last `s.windows` windows (frames = windows: one
// kick/join per game update with ambient traffic). `window` is the id of the
// last window in the interval, `kick_dt` its kick-time delta.
inline std::string Line(const Stats &s, uint64_t window, float kick_dt, bool probe,
                        bool snapshot) {
  const uint64_t split_total = s.split_reads[0] + s.split_reads[1] + s.split_reads[2];
  char head[640];
  std::snprintf(head, sizeof(head),
                "MCLA_AMBDT v=1 probe=%d snapshot=%d window=%llu frames=%llu windows=%llu "
                "overlaps=%llu racer_splits=%llu racer_outside=%llu worker_steps=%llu "
                "split_reads=%llu reads=%s split=%s outside=%s served=%llu/%llu "
                "max_abs_dt_delta=%.6f min_ratio=%.4f kick_dt=%.6f join_changed=%llu "
                "kick_split=%llu last=",
                probe ? 1 : 0, snapshot ? 1 : 0, static_cast<unsigned long long>(window),
                static_cast<unsigned long long>(s.windows),
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
  return std::string(head) + MismatchList(s);
}

// The F10 "ambient_dt" object (session totals).
inline std::string StateJson(const Stats &s, uint64_t window, float kick_dt, bool probe,
                             bool snapshot) {
  char text[1024];
  std::snprintf(text, sizeof(text),
                "{\"version\": 1, \"probe\": %s, \"snapshot\": %s, \"window\": %llu, "
                "\"kick_dt\": %.6f, \"windows\": %llu, \"overlaps\": %llu, "
                "\"racer_splits\": %llu, \"racer_outside\": %llu, \"worker_steps\": %llu, "
                "\"reads\": [%llu, %llu, %llu], \"split_reads\": [%llu, %llu, %llu], "
                "\"outside\": [%llu, %llu, %llu], \"served\": [%llu, %llu], "
                "\"max_abs_dt_delta\": %.6f, \"min_ratio\": %.4f, \"join_changed\": %llu, "
                "\"kick_split\": %llu}",
                probe ? "true" : "false", snapshot ? "true" : "false",
                static_cast<unsigned long long>(window), double(kick_dt),
                static_cast<unsigned long long>(s.windows),
                static_cast<unsigned long long>(s.overlaps),
                static_cast<unsigned long long>(s.racer_splits),
                static_cast<unsigned long long>(s.racer_outside),
                static_cast<unsigned long long>(s.worker_steps),
                static_cast<unsigned long long>(s.reads[0]),
                static_cast<unsigned long long>(s.reads[1]),
                static_cast<unsigned long long>(s.reads[2]),
                static_cast<unsigned long long>(s.split_reads[0]),
                static_cast<unsigned long long>(s.split_reads[1]),
                static_cast<unsigned long long>(s.split_reads[2]),
                static_cast<unsigned long long>(s.outside[0]),
                static_cast<unsigned long long>(s.outside[1]),
                static_cast<unsigned long long>(s.outside[2]),
                static_cast<unsigned long long>(s.served[0]),
                static_cast<unsigned long long>(s.served[1]), s.max_abs_delta,
                s.has_ratio ? s.min_ratio : 1.0, static_cast<unsigned long long>(s.join_changed),
                static_cast<unsigned long long>(s.kick_split));
  return text;
}

}  // namespace mcla::ambient_dt

// The F10 state block "ambient_dt"; empty when both switches are off.
std::string MclaAmbientDtStateJson();
