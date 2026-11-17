#pragma once

// KI-082 traffic motion probe (MCLA_TRAFMOT v=1): pure parts of the
// default-off measurement arm mcla_traffic_motion_probe (owner key
// traffic_motion_probe). The hooks are in mcla_traffic_motion.cpp; everything
// here is unit-tested (tests/mcla_traffic_motion_test.cpp, which also checks
// the addresses and offsets against the generated code). Design and limits:
// docs/evidence/M11-l107-guest-tiling-and-msaa-20261117.md (section KI-082).
//
// Question: do nearby traffic cars "tremble" because their simulated
// positions jitter (uneven step, physics), or because a smooth simulation is
// shown with uneven steps against the presented frames, or only in the
// rendered pose (render side)?
//
// Static analysis (generated/default at the M11 codegen, our own reading):
// - sub_826FA0F8 (TU55, mcAmbientCollider vfunc +56; KI-054 site B is in
//   it) runs once per physically simulated ambient car per step on the main
//   thread (l84 probe: inline ambient step, one per timer update). r3 = the
//   collider; +672 = its physics instance. It picks the instance's matrix
//   exactly as follows and passes it to sub_826E5540: when the thread's TLS
//   byte [[r13] + 72] is set, inst + (word 0x827E5640 + 4) * 64 (a buffered
//   copy); otherwise, when inst byte +500 has bit 0x20 and inst +52 -> +48
//   is non-null, [that +28] + 16; otherwise inst + 128.
// - The matrix is read as four 16-byte rows (rage-style 3x4 with padding):
//   rows 0..2 the orientation, row 3 (+48) the position. The probe checks
//   the orientation rows are unit length (matrix_ok) and that the position
//   is finite and within +-100 km; a car failing either is not tracked.
// - The step: the main timer 0x827D7500 +8 after each tick (the raw timer
//   override calls MclaTrafficMotionOnTimerUpdate after sub_821BDA90). The
//   game runs one variable step per frame (no fixed-step accumulator, so
//   there is no interpolation alpha to log at the game-loop level).
//
// Per car (at most kMaxCars, by collider pointer) the probe keeps the
// position at the first collider call of each timer update U. The
// displacement d(U) = p(U) - p(U-1) is the motion of update U-1, made with
// the step dt(U-1) and shown for the host interval host(U-1) (time between
// the two timer ticks). Speeds: v_sim = |d| / dt (simulation), v_vis = |d| /
// host (what the eye sees). Between consecutive displacements of one car:
//   vr_sim = v_sim / v_sim(prev), vr_vis = v_vis / v_vis(prev),
//   dt_ratio = dt(U-1) / dt(U-2), reversal = d . d(prev) < 0 (both moves
//   above kMinMoveM; a back-and-forth, also while standing).
// Classification of one sample (ratios only above kMinSpeed):
//   sim_jitter: |vr_sim - 1| > kJitter with an even step (|dt_ratio - 1| <
//     kEvenStep): the simulated motion itself is uneven;
//   vis_jitter: |vr_vis - 1| > kJitter while |vr_sim - 1| <= kSmooth: a
//     smooth simulation shown unevenly (step versus presented interval);
// A tremble seen with neither (and no reversals) is on the render side
// (pose interpolation or skinning), which this probe does not see.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace mcla::traffic_motion {

// The collider update and how it picks its matrix.
inline constexpr uint32_t kCollider = 0x826FA0F8u;
inline constexpr uint32_t kInstOffset = 672;
inline constexpr uint32_t kInstFlagsOffset = 500;
inline constexpr uint8_t kInstLinkedBit = 0x20;
inline constexpr uint32_t kInstLinkOffset = 52;
inline constexpr uint32_t kLinkInnerOffset = 48;
inline constexpr uint32_t kInnerBaseOffset = 28;
inline constexpr uint32_t kInstBaseOffset = 112;
inline constexpr uint32_t kMatrixFromBase = 16;
inline constexpr uint32_t kTlsByteOffset = 72;
inline constexpr uint32_t kBufferIndex = 0x827E5640u;  // lis -32130 / 22080
inline constexpr uint32_t kBufferFirst = 4;
inline constexpr uint32_t kMatrixBytes = 64;
inline constexpr uint32_t kRowBytes = 16;
inline constexpr uint32_t kPositionOffset = 48;
// The main timer and its step.
inline constexpr uint32_t kMainTimer = 0x827D7500u;
inline constexpr uint32_t kDeltaOffset = 8;

enum class Path : uint8_t { kInstance, kLinked, kBuffered, kCount };

inline const char *PathName(Path path) {
  switch (path) {
    case Path::kInstance:
      return "instance";
    case Path::kLinked:
      return "linked";
    case Path::kBuffered:
      return "buffered";
    default:
      return "unknown";
  }
}

// The branch sub_826FA0F8 takes (0x826FA128 .. 0x826FA178); `inner` is inst
// +52 -> +48 (0 when +52 is 0).
constexpr Path SelectPath(uint8_t tls_byte, uint8_t inst_flags, uint32_t inner) {
  if (tls_byte != 0) {
    return Path::kBuffered;
  }
  if ((inst_flags & kInstLinkedBit) != 0 && inner != 0) {
    return Path::kLinked;
  }
  return Path::kInstance;
}

// The matrix address for a path (inner_base = [inner + 28] for kLinked).
constexpr uint32_t MatrixAddress(Path path, uint32_t inst, uint32_t inner_base,
                                 uint32_t buffer_index) {
  switch (path) {
    case Path::kBuffered:
      return inst + ((buffer_index + kBufferFirst) << 6);
    case Path::kLinked:
      return inner_base + kMatrixFromBase;
    default:
      return inst + kInstBaseOffset + kMatrixFromBase;
  }
}

// A guest pointer the hook may read a 64-byte matrix or a field through.
constexpr bool PointerPlausible(uint32_t p) {
  return p >= 0x00010000u && p <= 0xFFFFFFFFu - 1024u;
}

struct Vec3 {
  float x = 0;
  float y = 0;
  float z = 0;
};

inline double Dot(const Vec3 &a, const Vec3 &b) {
  return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
}
inline double Length(const Vec3 &a) {
  return std::sqrt(Dot(a, a));
}
inline Vec3 Sub(const Vec3 &a, const Vec3 &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// Orientation rows of unit length (+-10 %).
inline bool MatrixOk(const Vec3 &r0, const Vec3 &r1, const Vec3 &r2) {
  for (const Vec3 *r : {&r0, &r1, &r2}) {
    const double l = Length(*r);
    if (!(l > 0.9 && l < 1.1)) {
      return false;
    }
  }
  return true;
}

inline bool PositionOk(const Vec3 &p) {
  for (const float c : {p.x, p.y, p.z}) {
    if (!std::isfinite(c) || std::fabs(c) > 1.0e5f) {
      return false;
    }
  }
  return true;
}

inline constexpr size_t kMaxCars = 8;
inline constexpr size_t kBurstCars = 6;
inline constexpr double kMinMoveM = 0.002;
inline constexpr double kMinSpeed = 1.0;   // m/s, for ratios
inline constexpr double kJitter = 0.25;
inline constexpr double kSmooth = 0.10;
inline constexpr double kEvenStep = 0.05;
inline constexpr double kDtJump = 0.10;
inline constexpr int64_t kMinuteNs = 60'000'000'000;
inline constexpr int64_t kBurstNs = 5'000'000'000;

// |ratio - 1| in steps of 0.01 (bin 100: 1.00 and above).
struct Histogram {
  std::array<uint32_t, 101> bins{};
  uint64_t count = 0;
  double max = 0;
  void Add(double dev) {
    dev = std::fabs(dev);
    const size_t i = std::min<size_t>(100, static_cast<size_t>(dev * 100.0));
    ++bins[i];
    ++count;
    max = std::max(max, dev);
  }
  // The upper edge of the bin holding the q quantile (0 when empty).
  double Quantile(double q) const {
    if (count == 0) {
      return 0;
    }
    const uint64_t target = static_cast<uint64_t>(std::ceil(q * double(count)));
    uint64_t seen = 0;
    for (size_t i = 0; i < bins.size(); ++i) {
      seen += bins[i];
      if (seen >= std::max<uint64_t>(target, 1)) {
        return double(i + 1) / 100.0;
      }
    }
    return 1.01;
  }
};

struct Sample {
  uint8_t slot = 0;
  uint32_t calls = 0;  // collider calls of the car in the update that moved it
  double dp = 0;       // metres
  double v_sim = 0;
  double v_vis = 0;
  bool ratios = false;
  double vr_sim = 0;
  double vr_vis = 0;
  bool reversal = false;
};

// One finished timer update (the displacements its first collider calls saw).
struct Frame {
  uint64_t update = 0;
  double dt = 0;        // step of the displacements (s)
  double host = 0;      // host interval they are shown for (s)
  double dt_ratio = 0;  // dt / previous dt
  uint32_t cars = 0;
  uint32_t calls = 0;
  std::array<Sample, kMaxCars> samples{};
  uint32_t sample_count = 0;
};

struct Minute {
  uint64_t updates = 0;
  double host_sum = 0;
  double host_sq = 0;
  double host_max = 0;
  double dt_sum = 0;
  double dt_min = 0;
  double dt_max = 0;
  uint64_t dt_jumps = 0;
  uint64_t samples = 0;
  uint64_t new_cars = 0;
  Histogram vr_sim;
  Histogram vr_vis;
  uint64_t reversals = 0;
  uint64_t sim_jitter = 0;
  uint64_t vis_jitter = 0;
  uint64_t multi_calls = 0;
  uint64_t bad_matrix = 0;
  uint64_t calls = 0;
  std::array<uint64_t, static_cast<size_t>(Path::kCount)> paths{};
};

class Tracker {
 public:
  // A main-timer tick (after it): now in ns, dt the game's new step.
  // Returns true and fills *finished with the update that just ended when
  // there was one.
  bool OnUpdate(int64_t now_ns, double dt, Frame *finished) {
    const bool had = update_ > 0;
    if (had && finished != nullptr) {
      *finished = frame_;
    }
    dt_prev2_ = dt_prev_;
    dt_prev_ = dt_cur_;
    dt_cur_ = dt;
    host_prev_ = last_ns_ != 0 && now_ns > last_ns_ ? double(now_ns - last_ns_) * 1e-9 : 0;
    last_ns_ = now_ns;
    ++update_;
    if (minute_start_ns_ == 0) {
      minute_start_ns_ = now_ns;
    }
    frame_ = Frame{};
    frame_.update = update_;
    frame_.dt = dt_prev_;
    frame_.host = host_prev_;
    frame_.dt_ratio = dt_prev2_ > 0 ? dt_prev_ / dt_prev2_ : 0;
    // Minute statistics of the step and the host interval.
    if (update_ > 1) {
      Minute &m = minute_;
      ++m.updates;
      m.host_sum += host_prev_;
      m.host_sq += host_prev_ * host_prev_;
      m.host_max = std::max(m.host_max, host_prev_);
      m.dt_sum += dt_prev_;
      m.dt_min = m.updates == 1 ? dt_prev_ : std::min(m.dt_min, dt_prev_);
      m.dt_max = std::max(m.dt_max, dt_prev_);
      if (dt_prev2_ > 0 && std::fabs(dt_prev_ / dt_prev2_ - 1.0) > kDtJump) {
        ++m.dt_jumps;
      }
    }
    return had;
  }

  // A collider call in the current update: the car's matrix position.
  void OnCar(uint32_t id, const Vec3 &pos, bool matrix_ok, Path path) {
    Minute &m = minute_;
    ++m.calls;
    ++m.paths[static_cast<size_t>(path)];
    ++frame_.calls;
    if (!matrix_ok || !PositionOk(pos)) {
      ++m.bad_matrix;
      return;
    }
    Car *car = Find(id);
    if (car == nullptr) {
      car = Allocate(id);
      ++m.new_cars;
      car->update = update_;
      car->pos = pos;
      car->calls = 1;
      ++frame_.cars;
      return;
    }
    if (car->update == update_) {
      ++car->calls;
      return;
    }
    ++frame_.cars;
    if (car->update + 1 != update_ || frame_.dt <= 0 || frame_.host <= 0) {
      Restart(*car, pos);
      return;
    }
    Sample s;
    s.slot = static_cast<uint8_t>(car - cars_.data());
    s.calls = car->calls;
    const Vec3 d = Sub(pos, car->pos);
    s.dp = Length(d);
    s.v_sim = s.dp / frame_.dt;
    s.v_vis = s.dp / frame_.host;
    if (car->has_d) {
      s.reversal = Dot(d, car->d) < 0 && s.dp > kMinMoveM && car->dp > kMinMoveM;
      if (car->v_sim > kMinSpeed && s.v_sim > kMinSpeed) {
        s.ratios = true;
        s.vr_sim = s.v_sim / car->v_sim;
        s.vr_vis = s.v_vis / car->v_vis;
      }
    }
    ++m.samples;
    m.reversals += s.reversal ? 1 : 0;
    m.multi_calls += s.calls > 1 ? 1 : 0;
    if (s.ratios) {
      const double sim = s.vr_sim - 1.0;
      const double vis = s.vr_vis - 1.0;
      m.vr_sim.Add(sim);
      m.vr_vis.Add(vis);
      const bool even = frame_.dt_ratio > 0 && std::fabs(frame_.dt_ratio - 1.0) < kEvenStep;
      if (std::fabs(sim) > kJitter && even) {
        ++m.sim_jitter;
      }
      if (std::fabs(vis) > kJitter && std::fabs(sim) <= kSmooth) {
        ++m.vis_jitter;
      }
    }
    if (frame_.sample_count < frame_.samples.size()) {
      frame_.samples[frame_.sample_count++] = s;
    }
    car->d = d;
    car->dp = s.dp;
    car->has_d = true;
    car->v_sim = s.v_sim;
    car->v_vis = s.v_vis;
    car->pos = pos;
    car->update = update_;
    car->calls = 1;
  }

  bool MinuteDue(int64_t now_ns) const {
    return minute_start_ns_ != 0 && now_ns - minute_start_ns_ >= kMinuteNs;
  }
  Minute TakeMinute(int64_t now_ns) {
    Minute out = minute_;
    minute_ = Minute{};
    minute_start_ns_ = now_ns;
    return out;
  }
  const Minute &minute() const { return minute_; }
  uint64_t update() const { return update_; }

 private:
  struct Car {
    bool used = false;
    uint32_t id = 0;
    uint64_t update = 0;
    Vec3 pos;
    bool has_d = false;
    Vec3 d;
    double dp = 0;
    double v_sim = 0;
    double v_vis = 0;
    uint32_t calls = 0;
  };

  Car *Find(uint32_t id) {
    for (Car &c : cars_) {
      if (c.used && c.id == id) {
        return &c;
      }
    }
    return nullptr;
  }
  // A free slot, else the one seen longest ago.
  Car *Allocate(uint32_t id) {
    Car *pick = nullptr;
    for (Car &c : cars_) {
      if (!c.used) {
        pick = &c;
        break;
      }
      if (pick == nullptr || c.update < pick->update) {
        pick = &c;
      }
    }
    *pick = Car{};
    pick->used = true;
    pick->id = id;
    return pick;
  }
  void Restart(Car &car, const Vec3 &pos) {
    const uint32_t id = car.id;
    car = Car{};
    car.used = true;
    car.id = id;
    car.update = update_;
    car.pos = pos;
    car.calls = 1;
  }

  std::array<Car, kMaxCars> cars_{};
  uint64_t update_ = 0;
  double dt_cur_ = 0;
  double dt_prev_ = 0;
  double dt_prev2_ = 0;
  double host_prev_ = 0;
  int64_t last_ns_ = 0;
  int64_t minute_start_ns_ = 0;
  Frame frame_;
  Minute minute_;
};

// Per-update burst line (F10: kBurstNs of them).
inline std::string FrameLine(const Frame &f) {
  char head[192];
  std::snprintf(head, sizeof(head),
                "MCLA_TRAFMOT v=1 kind=frame upd=%llu dt_ms=%.3f host_ms=%.3f dt_ratio=%.3f "
                "cars=%u calls=%u",
                static_cast<unsigned long long>(f.update), f.dt * 1e3, f.host * 1e3, f.dt_ratio,
                unsigned(f.cars), unsigned(f.calls));
  std::string out = head;
  const uint32_t n = std::min<uint32_t>(f.sample_count, kBurstCars);
  for (uint32_t i = 0; i < n; ++i) {
    const Sample &s = f.samples[i];
    char car[96];
    std::snprintf(car, sizeof(car), " c%u=%u:%.1f:%.2f:%.3f:%.3f:%d", unsigned(s.slot),
                  unsigned(s.calls), s.dp * 1e3, s.v_sim, s.ratios ? s.vr_sim : 0.0,
                  s.ratios ? s.vr_vis : 0.0, s.reversal ? 1 : 0);
    out += car;
  }
  return out;
}

// Per-minute summary (always while the probe is on).
inline std::string MinuteLine(const Minute &m, bool burst) {
  const double n = double(std::max<uint64_t>(m.updates, 1));
  const double mean = m.host_sum / n;
  const double sd = std::sqrt(std::max(0.0, m.host_sq / n - mean * mean));
  char text[768];
  std::snprintf(
      text, sizeof(text),
      "MCLA_TRAFMOT v=1 kind=minute updates=%llu host_ms=%.2f/%.2f/%.2f dt_ms=%.2f/%.2f/%.2f "
      "dt_jumps=%llu samples=%llu new_cars=%llu ratios=%llu vr_sim_dev=%.2f/%.2f/%.3f "
      "vr_vis_dev=%.2f/%.2f/%.3f reversals=%llu sim_jitter=%llu vis_jitter=%llu "
      "multi_calls=%llu calls=%llu bad_matrix=%llu paths=%llu/%llu/%llu burst=%d",
      static_cast<unsigned long long>(m.updates), mean * 1e3, sd * 1e3, m.host_max * 1e3,
      m.dt_min * 1e3, m.dt_sum / n * 1e3, m.dt_max * 1e3,
      static_cast<unsigned long long>(m.dt_jumps), static_cast<unsigned long long>(m.samples),
      static_cast<unsigned long long>(m.new_cars),
      static_cast<unsigned long long>(m.vr_sim.count), m.vr_sim.Quantile(0.5),
      m.vr_sim.Quantile(0.95), m.vr_sim.max, m.vr_vis.Quantile(0.5), m.vr_vis.Quantile(0.95),
      m.vr_vis.max, static_cast<unsigned long long>(m.reversals),
      static_cast<unsigned long long>(m.sim_jitter), static_cast<unsigned long long>(m.vis_jitter),
      static_cast<unsigned long long>(m.multi_calls), static_cast<unsigned long long>(m.calls),
      static_cast<unsigned long long>(m.bad_matrix),
      static_cast<unsigned long long>(m.paths[0]), static_cast<unsigned long long>(m.paths[1]),
      static_cast<unsigned long long>(m.paths[2]), burst ? 1 : 0);
  return text;
}

// The F10 "traffic_motion" object (empty when the probe is off).
inline std::string StateJson(bool on, uint64_t updates, uint64_t minutes, uint64_t bursts,
                             uint64_t frame_lines, const Minute &m) {
  if (!on) {
    return {};
  }
  char text[512];
  std::snprintf(text, sizeof(text),
                "{\"version\": 1, \"updates\": %llu, \"minutes\": %llu, \"bursts\": %llu, "
                "\"frame_lines\": %llu, \"minute_now\": {\"updates\": %llu, \"samples\": %llu, "
                "\"reversals\": %llu, \"sim_jitter\": %llu, \"vis_jitter\": %llu, "
                "\"bad_matrix\": %llu}}",
                static_cast<unsigned long long>(updates), static_cast<unsigned long long>(minutes),
                static_cast<unsigned long long>(bursts),
                static_cast<unsigned long long>(frame_lines),
                static_cast<unsigned long long>(m.updates),
                static_cast<unsigned long long>(m.samples),
                static_cast<unsigned long long>(m.reversals),
                static_cast<unsigned long long>(m.sim_jitter),
                static_cast<unsigned long long>(m.vis_jitter),
                static_cast<unsigned long long>(m.bad_matrix));
  return text;
}

}  // namespace mcla::traffic_motion

// Raw timer (mcla_raw_timer.cpp), after each main-timer tick: the update
// boundary (a no-op unless mcla_traffic_motion_probe is on).
void MclaTrafficMotionOnTimerUpdate(uint8_t *base);

// An accepted live snapshot (F10 and the other triggers): start a burst of
// per-update lines (a no-op unless the probe is on).
void MclaTrafficMotionMark();

// The F10 state block "traffic_motion" (empty when the probe is off).
std::string MclaTrafficMotionStateJson();
