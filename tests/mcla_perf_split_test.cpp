#include "mcla_perf_split.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::perf_split::DecideDefault;
using mcla::perf_split::DefaultAction;
using mcla::perf_split::FormatJsonObject;
using mcla::perf_split::FormatThreadCpuLine;
using mcla::perf_split::JsonMs;
using mcla::perf_split::SanitizeThreadName;
using mcla::perf_split::ThreadCpuSample;

void TestDefaultDecision() {
  Check(DecideDefault(false, true, false) == DefaultAction::kDisabledByOption,
        "option off never writes the cvar");
  Check(DecideDefault(false, false, false) == DefaultAction::kDisabledByOption,
        "option off wins over a missing cvar");
  Check(DecideDefault(true, false, false) == DefaultAction::kUnavailable,
        "SDK without the cvar is reported");
  Check(DecideDefault(true, true, true) == DefaultAction::kUserSet, "user value is kept");
  Check(DecideDefault(true, true, false) == DefaultAction::kEnable, "l53 default enables");
}

ThreadCpuSample Sample(uint32_t id, uint64_t user_ms, uint64_t kernel_ms, std::string name) {
  ThreadCpuSample s;
  s.id = id;
  s.user_100ns = user_ms * 10000;
  s.kernel_100ns = kernel_ms * 10000;
  s.name = std::move(name);
  return s;
}

void TestThreadCpuDeltasAndOrder() {
  const std::vector<ThreadCpuSample> previous = {
      Sample(10, 1000, 100, "GPU Commands"),
      Sample(20, 5000, 0, "XThread0014 (F80004E0)"),
      Sample(30, 10, 10, "D3D12 Replay"),
  };
  const std::vector<ThreadCpuSample> current = {
      Sample(10, 31000, 1100, "GPU Commands"),        // 30 s + 1 s
      Sample(20, 64000, 0, "XThread0014 (F80004E0)"),  // 59 s
      Sample(30, 27010, 10, "D3D12 Replay"),           // 27 s
      Sample(40, 500, 0, "new, 'odd' name"),           // new thread: 0.5 s
  };
  const auto line = FormatThreadCpuLine("minute", 3, 60000.04, previous, current, 3);
  Check(line ==
            "MCLA_THREAD_CPU v=1 reason=minute seq=3 wall_ms=60000.0 threads=4 top=3"
            " thread=20,'XThread0014 (F80004E0)',59000.0,0.0"
            " thread=10,'GPU Commands',30000.0,1000.0"
            " thread=30,'D3D12 Replay',27000.0,0.0",
        "deltas sorted by total CPU, limited to top");
  const auto all = FormatThreadCpuLine("f10", 4, 1.0, previous, current, 8);
  Check(all.find(" top=4 ") != std::string::npos, "top is bounded by the thread count");
  Check(all.find(" thread=40,'new? ?odd? name',500.0,0.0") != std::string::npos,
        "a new thread counts from zero and its name is sanitized");
}

void TestThreadCpuTiesAndClockSkew() {
  const std::vector<ThreadCpuSample> previous = {Sample(2, 100, 0, "b"), Sample(1, 100, 0, "a")};
  // Thread 1 reports less than before (reused id): clamped to 0.
  const std::vector<ThreadCpuSample> current = {Sample(2, 100, 0, "b"), Sample(1, 50, 0, "a")};
  const auto line = FormatThreadCpuLine("minute", 1, 0.0, previous, current);
  Check(line.ends_with(" top=2 thread=1,'a',0.0,0.0 thread=2,'b',0.0,0.0"),
        "ties sort by id and negative deltas clamp to zero");
}

void TestNameSanitizing() {
  Check(SanitizeThreadName("a'b,c\td") == "a?b?c?d", "quotes, commas, controls replaced");
  Check(SanitizeThreadName(std::string(80, 'x')).size() == 48, "names are bounded");
}

void TestJson() {
  Check(FormatJsonObject({}) == "{}", "empty object");
  Check(FormatJsonObject({{"seq", "2"}, {"last_reason", "\"f10\""}, {"draw_ms", JsonMs(1.23456)}}) ==
            "{\"seq\": 2, \"last_reason\": \"f10\", \"draw_ms\": 1.235}",
        "fields in order");
  Check(JsonMs(-1.0) == "-1.000", "negative values stay");
  const double nan = std::strtod("nan", nullptr);
  Check(JsonMs(nan) == "0.000", "NaN becomes 0");
  Check(JsonMs(1e300) == "0.000", "huge values become 0");
}
}  // namespace

int main() {
  TestDefaultDecision();
  TestThreadCpuDeltasAndOrder();
  TestThreadCpuTiesAndClockSkew();
  TestNameSanitizing();
  TestJson();
  std::printf("mcla_perf_split_test: %d checks passed\n", checks);
  return 0;
}
