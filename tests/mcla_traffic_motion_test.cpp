// KI-082 traffic motion probe: pure parts of the default-off MCLA_TRAFMOT
// measurement (src/mcla_traffic_motion.h) and the generated collider code
// whose matrix branch the hook mirrors (MCLA_TRAFMOT_GENERATED_DIR).

#include "mcla_traffic_motion.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_TRAFMOT_GENERATED_DIR
#error "MCLA_TRAFMOT_GENERATED_DIR (generated/default) is set by CMake"
#endif

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

bool Has(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

namespace tmo = mcla::traffic_motion;
using tmo::Path;
using tmo::Vec3;

void TestLayout() {
  static_assert(tmo::kBufferIndex == 0x827E0000u + 22080u, "lis -32130 / 22080");
  static_assert(tmo::kMainTimer == 0x827D0000u + 29952u, "lis -32131 / 29952");
  static_assert(tmo::kInstBaseOffset + tmo::kMatrixFromBase == 128);
  static_assert(tmo::kPositionOffset == 3 * tmo::kRowBytes && tmo::kMatrixBytes == 64);
  static_assert(tmo::SelectPath(1, 0, 0) == Path::kBuffered && tmo::SelectPath(1, 0x20, 5) == Path::kBuffered,
                "TLS byte set: the buffered copy, whatever the flags");
  static_assert(tmo::SelectPath(0, 0x20, 0x40001000u) == Path::kLinked);
  static_assert(tmo::SelectPath(0, 0x20, 0) == Path::kInstance && tmo::SelectPath(0, 0xDF, 7) == Path::kInstance,
                "linked only with bit 0x20 and a non-null inner object");
  static_assert(tmo::MatrixAddress(Path::kInstance, 0x40000000u, 0, 0) == 0x40000080u);
  static_assert(tmo::MatrixAddress(Path::kBuffered, 0x40000000u, 0, 0) == 0x40000100u &&
                tmo::MatrixAddress(Path::kBuffered, 0x40000000u, 0, 2) == 0x40000180u,
                "(index + 4) * 64");
  static_assert(tmo::MatrixAddress(Path::kLinked, 0x40000000u, 0x50000000u, 0) == 0x50000010u);
  Check(tmo::PointerPlausible(0x40000000u) && !tmo::PointerPlausible(0) &&
            !tmo::PointerPlausible(0xFFFFFF00u),
        "pointer plausibility");
  Check(std::string(tmo::PathName(Path::kInstance)) == "instance" &&
            std::string(tmo::PathName(Path::kLinked)) == "linked" &&
            std::string(tmo::PathName(Path::kBuffered)) == "buffered",
        "path names");
  Check(tmo::MatrixOk({1, 0, 0}, {0, 1, 0}, {0, 0, 1}) &&
            tmo::MatrixOk({0.6f, 0.8f, 0}, {-0.8f, 0.6f, 0}, {0, 0, 1.05f}) &&
            !tmo::MatrixOk({0, 0, 0}, {0, 1, 0}, {0, 0, 1}) &&
            !tmo::MatrixOk({1, 0, 0}, {0, 2, 0}, {0, 0, 1}) &&
            !tmo::MatrixOk({NAN, 0, 0}, {0, 1, 0}, {0, 0, 1}),
        "orientation rows of unit length");
  Check(tmo::PositionOk({100, -20, 3000}) && !tmo::PositionOk({2e5f, 0, 0}) &&
            !tmo::PositionOk({INFINITY, 0, 0}) && !tmo::PositionOk({0, NAN, 0}),
        "position range");
}

void TestHistogram() {
  tmo::Histogram h;
  Check(h.Quantile(0.5) == 0 && h.count == 0, "empty histogram");
  for (int i = 0; i < 100; ++i) {
    h.Add(i < 95 ? 0.004 : -0.30);
  }
  Check(h.count == 100 && std::fabs(h.max - 0.30) < 1e-12, "count and max of |dev|");
  Check(std::fabs(h.Quantile(0.5) - 0.01) < 1e-12 && std::fabs(h.Quantile(0.95) - 0.01) < 1e-12 &&
            std::fabs(h.Quantile(0.96) - 0.31) < 1e-12,
        "quantiles are bin upper edges");
  tmo::Histogram big;
  big.Add(5.0);
  Check(std::fabs(big.Quantile(0.5) - 1.01) < 1e-12, "overflow bin");
}

constexpr int64_t kMs = 1'000'000;

// Drives one car through updates: host intervals (ms), steps (s) and the
// car's x position at each update's first collider call.
struct Run {
  tmo::Tracker t;
  int64_t now = 1000 * kMs;
  std::vector<tmo::Frame> frames;
  void Update(double host_ms, double dt) {
    now += static_cast<int64_t>(host_ms * kMs);
    tmo::Frame f;
    if (t.OnUpdate(now, dt, &f)) {
      frames.push_back(f);
    }
  }
  void Car(uint32_t id, float x, bool ok = true) {
    t.OnCar(id, Vec3{x, 2.0f, -5.0f}, ok, Path::kInstance);
  }
};

void TestSmooth() {
  Run r;
  float x = 0;
  for (int i = 0; i < 20; ++i) {
    r.Update(33.333, 1.0 / 30);
    r.Car(0x40001000u, x);
    x += 10.0f / 30;  // 10 m/s
  }
  const tmo::Minute &m = r.t.minute();
  Check(m.samples == 19 && m.new_cars == 1 && m.reversals == 0 && m.sim_jitter == 0 &&
            m.vis_jitter == 0 && m.vr_sim.count == 18 && m.vr_sim.max < 0.01 &&
            m.vr_vis.max < 0.01 && m.multi_calls == 0 && m.bad_matrix == 0 && m.calls == 20,
        "a car at a steady 10 m/s: no reversal, no jitter");
  Check(m.updates == 19 && m.dt_jumps == 0 && std::fabs(m.dt_min - 1.0 / 30) < 1e-9 &&
            std::fabs(m.dt_max - 1.0 / 30) < 1e-9,
        "step statistics");
  Check(r.frames.size() == 19 && r.frames.back().sample_count == 1 && r.frames.back().cars == 1,
        "one finished frame per update");
  const tmo::Sample &s = r.frames.back().samples[0];
  Check(s.ratios && std::fabs(s.v_sim - 10.0) < 1e-3 && std::fabs(s.dp - 1.0 / 3) < 1e-4 &&
            s.calls == 1 && !s.reversal,
        "the sample: 10 m/s over one step, one call");
  const std::string line = tmo::FrameLine(r.frames.back());
  Check(Has(line, "MCLA_TRAFMOT v=1 kind=frame upd=") && Has(line, " dt_ms=33.333 host_ms=33.333") &&
            Has(line, " dt_ratio=1.000 cars=1 calls=1 c0=1:333.3:10.00:1.000:1.000:0"),
        "the frame line");
}

void TestReversal() {
  Run r;
  const float xs[] = {0, 0.010f, 0.004f, 0.012f, 0.005f, 0.013f};
  for (float x : xs) {
    r.Update(33.333, 1.0 / 30);
    r.Car(7, x);
  }
  const tmo::Minute &m = r.t.minute();
  Check(m.samples == 5 && m.reversals == 4 && m.vr_sim.count == 0,
        "a car standing and trembling by mm: reversals, no ratios below 1 m/s");
}

void TestVisJitter() {
  // Smooth simulation (v = 12 m/s at an even 40 ms step) shown for host
  // intervals that alternate 33 / 50 ms: step versus presented interval.
  Run r;
  float x = 0;
  for (int i = 0; i < 12; ++i) {
    r.Update(i % 2 ? 50.0 : 33.333, 0.040);
    r.Car(9, x);
    x += 12.0f * 0.040f;
  }
  const tmo::Minute &m = r.t.minute();
  Check(m.vis_jitter > 0 && m.sim_jitter == 0 && m.reversals == 0 && m.vr_sim.max < 0.01 &&
            m.vr_vis.max > 0.3,
        "smooth simulation shown unevenly: vis_jitter only");
}

void TestSimJitter() {
  // An even step with displacements alternating 0.30 / 0.45 m: the simulated
  // motion itself is uneven.
  Run r;
  float x = 0;
  for (int i = 0; i < 12; ++i) {
    r.Update(33.333, 1.0 / 30);
    r.Car(11, x);
    x += i % 2 ? 0.45f : 0.30f;
  }
  const tmo::Minute &m = r.t.minute();
  Check(m.sim_jitter > 0 && m.reversals == 0, "uneven simulated motion at an even step: sim_jitter");
  Check(m.vis_jitter == 0, "and not vis_jitter (the simulation is not smooth)");
}

void TestStepJumpAndGaps() {
  Run r;
  float x = 0;
  for (int i = 0; i < 10; ++i) {
    const double dt = i == 5 ? 0.1 : 1.0 / 30;  // a hitch step
    r.Update(i == 5 ? 100.0 : 33.333, dt);
    r.Car(3, x);
    x += static_cast<float>(10.0 * dt);
  }
  const tmo::Minute &m = r.t.minute();
  Check(m.dt_jumps == 2 && m.sim_jitter == 0 && m.reversals == 0,
        "a hitch step moved consistently: two step jumps, no simulation jitter");
  Check(m.vis_jitter == 3,
        "shown against the host intervals the hitch is a visual jump (three samples)");
  // A gap (the car not seen for an update) restarts its history; a bad
  // matrix is counted and not tracked; two calls in one update are one
  // multi-call sample.
  Run g;
  g.Update(33.333, 1.0 / 30);
  g.Car(5, 0.0f);
  g.Update(33.333, 1.0 / 30);
  g.Car(5, 0.3f);
  g.Update(33.333, 1.0 / 30);  // not seen
  g.Update(33.333, 1.0 / 30);
  g.Car(5, 1.2f);
  Check(g.t.minute().samples == 1, "a gap restarts the car (no sample across it)");
  g.Car(6, 0.0f, false);
  Check(g.t.minute().bad_matrix == 1 && g.t.minute().new_cars == 1, "a bad matrix is not tracked");
  g.Car(5, 1.25f);  // second call of car 5 in this update
  g.Update(33.333, 1.0 / 30);
  g.Car(5, 1.5f);
  Check(g.t.minute().multi_calls == 1 && g.t.minute().samples == 2, "two calls in one update");
}

void TestSlotsAndMinute() {
  Run r;
  for (int u = 0; u < 3; ++u) {
    r.Update(33.333, 1.0 / 30);
    for (uint32_t id = 1; id <= 10; ++id) {
      r.Car(id, static_cast<float>(u) * 0.3f + static_cast<float>(id));
    }
  }
  Check(r.t.minute().new_cars >= 10, "ten cars into eight slots: evictions");
  Check(r.frames.back().cars <= tmo::kMaxCars + 2, "frame car count bounded");
  const tmo::Frame &f = r.frames.back();
  const std::string line = tmo::FrameLine(f);
  size_t cars = 0;
  for (size_t at = line.find(" c"); at != std::string::npos; at = line.find(" c", at + 1)) {
    cars += line[at + 2] >= '0' && line[at + 2] <= '9' ? 1 : 0;
  }
  Check(cars <= tmo::kBurstCars, "at most six cars per frame line");

  tmo::Tracker t;
  int64_t now = 5 * kMs;
  t.OnUpdate(now, 1.0 / 30, nullptr);
  Check(!t.MinuteDue(now + tmo::kMinuteNs - 1) && t.MinuteDue(now + tmo::kMinuteNs),
        "a summary every minute");
  now += 40 * kMs;
  t.OnUpdate(now, 1.0 / 30, nullptr);
  const tmo::Minute m = t.TakeMinute(now);
  Check(m.updates == 1 && t.minute().updates == 0 && !t.MinuteDue(now + 1), "taken and reset");
  const std::string line2 = tmo::MinuteLine(m, true);
  Check(Has(line2, "MCLA_TRAFMOT v=1 kind=minute updates=1 host_ms=40.00/0.00/40.00 "
                   "dt_ms=33.33/33.33/33.33 dt_jumps=0 samples=0 new_cars=0 ratios=0") &&
            Has(line2, " paths=0/0/0 burst=1"),
        "the minute line");
  Check(tmo::StateJson(false, 1, 2, 3, 4, m).empty(), "F10: nothing while off");
  const std::string json = tmo::StateJson(true, 10, 2, 1, 150, m);
  Check(Has(json, "{\"version\": 1, \"updates\": 10, \"minutes\": 2, \"bursts\": 1, "
                  "\"frame_lines\": 150, \"minute_now\": {\"updates\": 1,") &&
            json.back() == '}',
        "F10 object");
}

// --- generated code --------------------------------------------------------------
std::string ReadFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string Body(const std::string &text, const std::string &name) {
  const std::string head = "DEFINE_REX_FUNC(" + name + ") {";
  const size_t begin = text.find(head);
  if (begin == std::string::npos) {
    return {};
  }
  const size_t end = text.find("DEFINE_REX_FUNC(", begin + head.size());
  return text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

void InOrder(const std::string &body, const std::vector<std::string> &lines, const char *what) {
  size_t from = 0;
  for (const auto &line : lines) {
    const size_t at = body.find(line, from);
    if (at == std::string::npos) {
      std::fprintf(stderr, "missing in %s: %s\n", what, line.c_str());
      Check(false, what);
    }
    from = at + line.size();
  }
  Check(true, what);
}

void TestGenerated() {
  const std::string dir = MCLA_TRAFMOT_GENERATED_DIR;
  const std::string g55 = ReadFile(dir + "/mcla_recomp.55.cpp");
  const std::string reg = ReadFile(dir + "/mcla_register.cpp");
  Check(!g55.empty() && !reg.empty(), "generated files readable");
  const std::string body = Body(g55, "sub_826FA0F8");
  InOrder(body,
          {"REX_FUNC_PROLOGUE(0x826FA0F8);", "// mr r16,r3", "// lwz r10,0(r13)", "// li r9,72",
           "// lwz r11,672(r16)", "// lbzx r8,r9,r10", "// beq cr6,0x826fa144",
           "// lis r10,-32130", "// lwz r10,22080(r10)", "// addi r9,r10,4",
           "// rlwinm r10,r9,6,0,25", "// add r4,r10,r11", "// b 0x826fa17c", "loc_826FA144:",
           "// lbz r10,500(r11)", "// rlwinm r9,r10,0,26,26", "// beq cr6,0x826fa174",
           "// lwz r10,52(r11)", "// beq cr6,0x826fa174", "// lwz r10,48(r10)",
           "// beq cr6,0x826fa174", "// lwz r11,28(r10)", "// b 0x826fa178", "loc_826FA174:",
           "// addi r11,r11,112", "loc_826FA178:", "// addi r4,r11,16", "loc_826FA17C:",
           "// bl 0x826e5540"},
          "collider: the instance +672 and the matrix branch (buffered / linked / +128)");
  InOrder(body, {"// addi r19,r11,29952", "// lfs f1,8(r19)", "// bl 0x826fba60"},
          "collider: the main timer step (KI-054 site B) inside the same function");
  // No direct caller: reached through the vtable, i.e. the function table.
  size_t direct = 0;
  for (int unit = 0; unit < 65; ++unit) {
    const std::string g = ReadFile(dir + "/mcla_recomp." + std::to_string(unit) + ".cpp");
    for (size_t at = g.find("sub_826FA0F8(ctx, base);"); at != std::string::npos;
         at = g.find("sub_826FA0F8(ctx, base);", at + 1)) {
      ++direct;
    }
  }
  Check(direct == 0 && Has(reg, "SetFunction(0x826FA0F8, sub_826FA0F8)"),
        "the collider is a virtual function registered by symbol (the override replaces it)");
}

}  // namespace

int main() {
  TestLayout();
  TestHistogram();
  TestSmooth();
  TestReversal();
  TestVisJitter();
  TestSimJitter();
  TestStepJumpAndGaps();
  TestSlotsAndMinute();
  TestGenerated();
  std::printf("mcla_traffic_motion_test: %d checks passed\n", checks);
  return 0;
}
