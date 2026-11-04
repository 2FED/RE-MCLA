#include "mcla_vram_census_win.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

bool Has(const std::string &line, std::string_view field) {
  return line.find(std::string(" ") + std::string(field) + " ") != std::string::npos ||
         (line.size() >= field.size() + 1 &&
          line.compare(line.size() - field.size() - 1, std::string::npos,
                       std::string(" ") + std::string(field)) == 0);
}

using namespace mcla::vram;

void TestScale() {
  Check(EffectiveDrawScale(1, false, 1, false, 1, false) == 1, "defaults: scale 1");
  Check(EffectiveDrawScale(1, false, 2, true, 2, true) == 2, "launcher 1080p/1440p: scale 2");
  Check(EffectiveDrawScale(3, true, 1, false, 1, false) == 3, "shared scale applies to unset axes");
  Check(EffectiveDrawScale(3, true, 2, true, 2, true) == 2, "explicit axes win over shared");
  Check(EffectiveDrawScale(1, false, 1, true, 3, true) == 3, "larger axis");
  Check(EffectiveDrawScale(0, true, 0, false, -4, false) == 1, "at least 1");
}

void TestStartLimits() {
  // The l67 8 GB laptop: budget 7,131 MB.
  {
    const StartLimits legacy = DecideStartLimits(7131, 1, kSoftCapDefaultMb);
    Check(legacy.soft_mb == 3565 && legacy.hard_mb == 5595 && legacy.floor_mb == 3565,
          "scale 1 (720p): legacy 3565/5595, floor 3565");
    Check(legacy.reason == SoftCapReason::kScale1 && legacy.legacy_soft_mb == 3565,
          "scale 1 reason");
  }
  {
    const StartLimits capped = DecideStartLimits(7131, 2, kSoftCapDefaultMb);
    Check(capped.soft_mb == 2048 && capped.floor_mb == 2048, "scale 2, 7131 MB: soft and floor 2048");
    Check(capped.hard_mb == 5595, "hard unchanged");
    Check(capped.legacy_soft_mb == 3565 && capped.reason == SoftCapReason::kCapped,
          "capped reason, uncapped 3565");
    const StartLimits scale3 = DecideStartLimits(7131, 3, kSoftCapDefaultMb);
    Check(scale3.soft_mb == 2048 && scale3.hard_mb == 5595, "scale 3 capped too");
  }
  {
    // Budget moved down within the 5,936..7,131 MB l48-l67 range.
    const StartLimits low = DecideStartLimits(5936, 2, kSoftCapDefaultMb);
    Check(low.soft_mb == 2048 && low.hard_mb == 4400 && low.floor_mb == 2048, "5936 MB budget");
  }
  {
    // 12 GB card (budget about 11.4 GB) and exactly 10 GB: unaffected.
    const StartLimits big = DecideStartLimits(11674, 2, kSoftCapDefaultMb);
    Check(big.reason == SoftCapReason::kLargeBudget && big.soft_mb == 4096 &&
              big.floor_mb == 4096 && big.hard_mb == 8192,
          "budget >= 10 GB unaffected");
    const StartLimits edge = DecideStartLimits(10240, 2, kSoftCapDefaultMb);
    Check(edge.reason == SoftCapReason::kLargeBudget && edge.soft_mb == 4096,
          "exactly 10 GB unaffected");
    const StartLimits under = DecideStartLimits(10239, 2, kSoftCapDefaultMb);
    Check(under.reason == SoftCapReason::kCapped && under.soft_mb == 2048,
          "just under 10 GB capped");
  }
  {
    // User override of the cap cvar (command line or the owner A/B key).
    const StartLimits off = DecideStartLimits(7131, 2, 0);
    Check(off.reason == SoftCapReason::kOff && off.soft_mb == 3565 && off.floor_mb == 3565 &&
              off.hard_mb == 5595,
          "cap 0 (soft_cap_off=true): legacy limits");
    const StartLimits user = DecideStartLimits(7131, 2, 3000);
    Check(user.reason == SoftCapReason::kCapped && user.soft_mb == 3000 && user.floor_mb == 3000,
          "user cap 3000 wins over the default");
    const StartLimits tiny = DecideStartLimits(7131, 2, 100);
    Check(tiny.soft_mb == kSoftMinMb && tiny.floor_mb == kSoftMinMb,
          "cap below the SDK minimum clamps to 384");
    const StartLimits above = DecideStartLimits(7131, 2, 5000);
    Check(above.reason == SoftCapReason::kNotBelow && above.soft_mb == 3565,
          "cap above budget/2: unchanged");
  }
  {
    // Small cards: budget/2 already under the cap.
    const StartLimits small = DecideStartLimits(3500, 2, kSoftCapDefaultMb);
    Check(small.reason == SoftCapReason::kNotBelow && small.soft_mb == 1750 &&
              small.hard_mb == 1964,
          "3.5 GB budget unchanged");
    const StartLimits tiny = DecideStartLimits(1000, 2, kSoftCapDefaultMb);
    Check(tiny.hard_mb == kHardMinMb && tiny.soft_mb == 500, "1 GB budget clamps");
  }
  Check(std::string_view(SoftCapReasonName(SoftCapReason::kCapped)) == "capped", "reason name");
}

void TestBudgetAction() {
  Check(DecideBudgetAction(true, true, true) == BudgetAction::kWine, "Wine first");
  Check(DecideBudgetAction(false, true, true) == BudgetAction::kUserSet,
        "user-set texture limits win (census only)");
  Check(DecideBudgetAction(false, false, true) == BudgetAction::kApply, "apply");
  Check(DecideBudgetAction(false, false, false) == BudgetAction::kUnavailable, "no budget");
  Check(DecideBudgetAction(false, true, false) == BudgetAction::kUnavailable,
        "no budget even when user set");
}

class FakeKmt final : public KmtApi {
 public:
  bool open_ok = true;
  int nodes = 3;
  int node_3d = 1;
  bool node_ok = true, adapter_ok = true;
  NodePerf node{1'500'000'000, 2'500'000'000};
  AdapterPerf adapter{7'000'000'000, 455, 712, 0};
  int opens = 0, closes = 0, node_queries = 0, adapter_queries = 0;
  uint32_t queried_node = 99;
  uint32_t luid_low = 0;
  int32_t luid_high = 0;

  bool Open(uint32_t low, int32_t high) override {
    ++opens;
    luid_low = low;
    luid_high = high;
    return open_ok;
  }
  bool NodeIs3D(uint32_t n, bool &is_3d) override {
    if (int(n) >= nodes) return false;
    is_3d = int(n) == node_3d;
    return true;
  }
  bool QueryNode(uint32_t n, NodePerf &out) override {
    ++node_queries;
    queried_node = n;
    if (!node_ok) return false;
    out = node;
    return true;
  }
  bool QueryAdapter(AdapterPerf &out) override {
    ++adapter_queries;
    if (!adapter_ok) return false;
    out = adapter;
    return true;
  }
  void Close() override { ++closes; }
};

void TestReader() {
  {
    FakeKmt fake;
    GpuPerfReader reader(&fake);
    Check(reader.Start(true, 1, 2) == KmtState::kWine && fake.opens == 0,
          "Wine: no KMT call at all");
    const GpuPerfSample s = reader.Sample();
    Check(!s.node_ok && !s.adapter_ok && fake.node_queries == 0, "Wine: no queries");
  }
  {
    GpuPerfReader reader(nullptr);
    Check(reader.Start(false, 1, 2) == KmtState::kOff, "no API: off");
    Check(!reader.Sample().node_ok, "no API: empty sample");
  }
  {
    FakeKmt fake;
    fake.open_ok = false;
    GpuPerfReader reader(&fake);
    Check(reader.Start(false, 7, -1) == KmtState::kOpenFailed && fake.opens == 1,
          "open failure reported");
    reader.Sample();
    Check(fake.node_queries == 0 && fake.closes == 0, "nothing queried or closed after a failed open");
  }
  {
    FakeKmt fake;
    {
      GpuPerfReader reader(&fake);
      Check(reader.Start(false, 0x1234, 5) == KmtState::kOk, "open ok");
      Check(fake.luid_low == 0x1234 && fake.luid_high == 5, "LUID passed through");
      Check(reader.node() == 1, "3D node found by metadata");
      const GpuPerfSample s = reader.Sample();
      Check(s.node_ok && s.adapter_ok && fake.queried_node == 1, "3D node queried");
      Check(s.node.frequency_hz == 1'500'000'000 && s.adapter.temperature_deci_c == 712,
            "values passed through");
    }
    Check(fake.closes == 1, "closed by the destructor");
  }
  {
    FakeKmt fake;
    fake.node_3d = -1;  // no 3D engine reported: node 0
    GpuPerfReader reader(&fake);
    reader.Start(false, 0, 0);
    Check(reader.node() == 0, "fallback node 0");
  }
  {
    FakeKmt fake;
    fake.node_ok = false;
    GpuPerfReader reader(&fake);
    reader.Start(false, 0, 0);
    for (int i = 0; i < 10; ++i) {
      const GpuPerfSample s = reader.Sample();
      Check(!s.node_ok && s.adapter_ok, "partial: adapter data only");
    }
    Check(reader.state() == KmtState::kOk, "partial data keeps sampling");
  }
  {
    FakeKmt fake;
    fake.node_ok = fake.adapter_ok = false;
    GpuPerfReader reader(&fake);
    reader.Start(false, 0, 0);
    for (uint32_t i = 0; i < GpuPerfReader::kMaxConsecutiveFailures; ++i) reader.Sample();
    Check(reader.state() == KmtState::kFailed && fake.closes == 1,
          "disabled and closed after repeated failures");
    const int queries = fake.node_queries;
    reader.Sample();
    Check(fake.node_queries == queries, "no queries once disabled");
    reader.Stop();
    Check(fake.closes == 1, "closed once");
  }
}

MemorySample Mem(uint64_t usage, uint64_t budget, uint64_t nonlocal = 900) {
  MemorySample m;
  m.local_usage_mb = usage;
  m.local_budget_mb = budget;
  m.local_available_mb = budget / 2;
  m.nonlocal_ok = true;
  m.nonlocal_usage_mb = nonlocal;
  m.nonlocal_budget_mb = 16000;
  return m;
}

void TestLine() {
  MonitorView view;
  view.soft_mb = 2048;
  view.hard_mb = 5595;
  view.floor_mb = 2048;
  view.near_budget_steps = 3;
  view.near_budget_seconds = 42;
  view.peak_mb = 6916;
  view.scale = 2;
  CensusWindow w;
  Check(w.TakeLine("minute", 1, 60, view, KmtState::kOk).empty(), "no line before a sample");
  w.AddMemory(Mem(6800, 7131));
  w.AddMemory(Mem(6900, 6721, 1200));
  GpuPerfSample g;
  g.node_ok = g.adapter_ok = true;
  g.node = {1'200'000'000, 2'500'000'000};
  g.adapter = {7'001'000'000, 400, 700, 0};
  w.AddGpu(g);
  g.node.frequency_hz = 1'800'000'000;
  g.adapter.temperature_deci_c = 860;
  g.adapter.power_tenth_pct = 600;
  w.AddGpu(g);
  const std::string line = w.TakeLine("minute", 1, 60.04, view, KmtState::kOk);
  std::fprintf(stdout, "%s\n", line.c_str());
  Check(line.rfind("MCLA_VRAM v=1 reason=minute seq=1 window_s=60.0 samples=2 ", 0) == 0,
        "line prefix");
  Check(Has(line, "local_mb=6900") && Has(line, "local_max_mb=6900") &&
            Has(line, "local_budget_mb=6721"),
        "local usage and budget");
  Check(Has(line, "local_budget_delta_mb=-410"), "budget delta since the first sample");
  Check(Has(line, "local_budget_min_mb=6721") && Has(line, "local_budget_max_mb=7131"),
        "budget range in the window");
  Check(Has(line, "local_avail_mb=3360"), "available for reservation");
  Check(Has(line, "nonlocal_mb=1200") && Has(line, "nonlocal_max_mb=1200") &&
            Has(line, "nonlocal_budget_mb=16000") && Has(line, "nonlocal_budget_delta_mb=0"),
        "non-local");
  Check(Has(line, "tex_soft_mb=2048") && Has(line, "tex_hard_mb=5595") &&
            Has(line, "tex_floor_mb=2048") && Has(line, "steps=on"),
        "limits as the monitor knows them");
  Check(Has(line, "near_budget_steps=3") && Has(line, "near_budget_s=42") &&
            Has(line, "peak_mb=6916") && Has(line, "scale=2") && Has(line, "kmt=ok"),
        "counters");
  Check(Has(line, "gpu_mhz=1200/1500/1800") && Has(line, "gpu_max_mhz=2500"), "GPU clock");
  Check(Has(line, "mem_mhz=7001/7001/7001"), "memory clock");
  Check(Has(line, "temp_c=70.0/78.0/86.0"), "temperature in deci-degrees");
  Check(Has(line, "power_pct=40.0/50.0/60.0"), "power in tenths of a percent");
  Check(Has(line, "fan_rpm=n/a"), "fan 0 = n/a");

  // Next window: deltas against the previous line; KMT fields reset.
  w.AddMemory(Mem(6950, 6600));
  const std::string next = w.TakeLine("f10", 2, 12.5, view, KmtState::kOk);
  Check(next.rfind("MCLA_VRAM v=1 reason=f10 seq=2 window_s=12.5 samples=1 ", 0) == 0,
        "mark line");
  Check(Has(next, "local_budget_delta_mb=-121") && Has(next, "local_budget_min_mb=6600") &&
            Has(next, "local_budget_max_mb=6600"),
        "delta since the last line");
  Check(Has(next, "gpu_mhz=n/a") && Has(next, "gpu_max_mhz=n/a") && Has(next, "temp_c=n/a") &&
            Has(next, "power_pct=n/a") && Has(next, "mem_mhz=n/a"),
        "no KMT sample in the window: n/a");
  w.AddMemory(Mem(6950, 6700));
  const std::string up = w.TakeLine("exit", 3, 1, view, KmtState::kFailed);
  Check(Has(up, "local_budget_delta_mb=+100") && Has(up, "kmt=failed"), "positive delta sign");

  // Driver leaves everything at 0, non-local unavailable, census-only view.
  CensusWindow z;
  MemorySample m = Mem(100, 200);
  m.nonlocal_ok = false;
  z.AddMemory(m);
  GpuPerfSample zero;
  zero.node_ok = zero.adapter_ok = true;
  z.AddGpu(zero);
  MonitorView user = view;
  user.steps_enabled = false;
  const std::string zl = z.TakeLine("Live F10!", 1, 0, user, KmtState::kWine);
  Check(zl.find("reason=Live?F10? ") != std::string::npos, "reason sanitized");
  Check(Has(zl, "nonlocal_mb=n/a") && Has(zl, "nonlocal_budget_delta_mb=n/a"), "non-local n/a");
  Check(Has(zl, "gpu_mhz=n/a") && Has(zl, "temp_c=n/a") && Has(zl, "fan_rpm=n/a"),
        "zero driver fields: n/a");
  Check(Has(zl, "steps=off") && Has(zl, "kmt=wine"), "census only");
  Check(SanitizeReason("") == "none" && SanitizeReason(std::string(40, 'a')).size() == 32,
        "reason bounds");
  for (const char c : zl) {
    Check(c >= 32 && c < 127, "ASCII single line");
  }
}

}  // namespace

int main() {
  TestScale();
  TestStartLimits();
  TestBudgetAction();
  TestReader();
  TestLine();
  std::printf("VRAM census: %d checks, 0 failures\n", checks);
  return 0;
}
