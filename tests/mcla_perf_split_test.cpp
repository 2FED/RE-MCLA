#include "mcla_perf_split.h"

#include "mcla_guest_spin.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
#include <intrin.h>
#elif defined(__x86_64__)
#include <x86intrin.h>
#endif
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
// M11 l100 guest spin census and backoff (mcla_guest_spin.h).
void TestSpinBackoff() {
  using mcla::guest_spin::BackoffThreshold;
  using mcla::guest_spin::kEpisodeGapTicks;
  using mcla::guest_spin::kHintsPerPoll;
  using mcla::guest_spin::SpinBackoff;
  Check(BackoffThreshold(0) == 0 && BackoffThreshold(-5) == 0 && BackoffThreshold(64) == 64 &&
            BackoffThreshold(1 << 30) == uint32_t(mcla::guest_spin::kBackoffMax),
        "backoff threshold: off, negative off, 64, clamped");
  {
    // Off: never yields however long the wait.
    SpinBackoff b;
    uint64_t now = 1000;
    for (int i = 0; i < 1000; ++i) {
      Check(!b.BeforePoll(now, 0), "threshold 0 never yields");
      now += 100;
      b.AfterPoll(now, false);
      now += 10;
    }
    Check(b.consecutive_hints == 1000ull * kHintsPerPoll, "the hints of one wait accumulate");
  }
  {
    // N = 64 (two polls): poll, poll, yield + poll, poll, yield + poll, ...
    SpinBackoff b;
    uint64_t now = 1000;
    std::vector<bool> yields;
    for (int i = 0; i < 8; ++i) {
      const bool y = b.BeforePoll(now, 64);
      yields.push_back(y);
      now += 100;
      b.AfterPoll(now, y);
      now += 10;
    }
    const std::vector<bool> expected = {false, false, true, false, true, false, true, false};
    Check(yields == expected, "N=64: a yield every second poll after the first 64 hints");
  }
  {
    // N = 100 (not a multiple of 32): the first yield before the fifth poll.
    SpinBackoff b;
    uint64_t now = 1;
    int first = -1;
    for (int i = 0; i < 10 && first < 0; ++i) {
      if (b.BeforePoll(now, 100)) {
        first = i;
        b.AfterPoll(now + 1, true);
      } else {
        b.AfterPoll(now + 1, false);
      }
      now += 2;
    }
    Check(first == 4, "N=100: yields once 128 >= 100 hints ran");
  }
  {
    // A gap ends the wait: a new wait starts without a yield.
    SpinBackoff b;
    uint64_t now = 1;
    for (int i = 0; i < 4; ++i) {
      b.BeforePoll(now, 64);
      b.AfterPoll(now + 1, false);
      now += 2;
    }
    Check(b.BeforePoll(now, 64), "a long wait yields");
    b.AfterPoll(now + 1, true);
    now += 1 + kEpisodeGapTicks + 1;
    Check(!b.BeforePoll(now, 64) && b.consecutive_hints == 0, "after a gap the count restarts");
    // A clock that went backwards also restarts the count.
    b.AfterPoll(now + 1, false);
    b.AfterPoll(now + 2, false);
    Check(!b.BeforePoll(now - 100, 64) && b.consecutive_hints == 0,
          "a TSC step backwards restarts the count");
    // Exactly at the gap the wait continues.
    SpinBackoff c;
    c.AfterPoll(10, false);
    c.AfterPoll(20, false);
    Check(c.BeforePoll(20 + kEpisodeGapTicks, 64), "a gap of exactly kEpisodeGapTicks continues");
  }
}

void TestSpinCensusAndLines() {
  using mcla::guest_spin::FormatSpinLines;
  using mcla::guest_spin::SpinCensus;
  using mcla::guest_spin::SpinRow;
  using mcla::guest_spin::SpinWindow;
  static SpinCensus census;
  auto *a = census.Register(100);
  auto *b = census.Register(200);
  Check(a && b && a != b, "two threads, two slots");
  const std::vector<SpinRow> empty = census.Read();
  Check(empty.size() == 2 && empty[0].polls == 0, "registered threads read with zero counts");
  const auto none = FormatSpinLines("minute", 1, 0, SpinWindow(empty, empty, 2800.0));
  Check(none.size() == 1 && none[0] == "MCLA_SPIN v=1 reason=minute seq=1 thread=none backoff=0",
        "no polls: the none line");
  for (int i = 0; i < 10; ++i) {
    a->Note(i % 5 == 0, 2800);
  }
  for (int i = 0; i < 30; ++i) {
    b->Note(false, 28000);
  }
  const std::vector<SpinRow> first = census.Read();
  const auto window = SpinWindow(empty, first, 2800.0);
  Check(window.size() == 2 && window[0].tid == 200 && window[0].polls == 30 &&
            window[0].hints == 960 && window[1].tid == 100 && window[1].yields == 2,
        "most hints first; hints = 32 per poll");
  const auto lines = FormatSpinLines("f10 mark", 7, 64, window);
  Check(lines.size() == 2 &&
            lines[0] == "MCLA_SPIN v=1 reason=f10_mark seq=7 thread=200 polls=30 hints=960 "
                        "yields=0 est_ms=0.30 backoff=64" &&
            lines[1] == "MCLA_SPIN v=1 reason=f10_mark seq=7 thread=100 polls=10 hints=320 "
                        "yields=2 est_ms=0.01 backoff=64",
        "MCLA_SPIN lines (est_ms from the TSC ticks)");
  // Deltas: only the new polls of the window; a thread without polls drops.
  for (int i = 0; i < 3; ++i) {
    a->Note(false, 0);
  }
  const auto second = SpinWindow(first, census.Read(), 2800.0);
  Check(second.size() == 1 && second[0].tid == 100 && second[0].polls == 3 &&
            second[0].yields == 0,
        "window deltas; idle threads drop out");
  // Top limit and ties.
  std::vector<mcla::guest_spin::SpinDelta> many(5);
  for (size_t i = 0; i < many.size(); ++i) {
    many[i].tid = uint32_t(10 + i);
    many[i].polls = 1;
    many[i].hints = 32;
  }
  Check(FormatSpinLines("m", 2, 0, many).size() == mcla::guest_spin::kSpinTop,
        "at most kSpinTop lines");
  std::vector<SpinRow> tie = {{7, 4, 0, 0}, {3, 4, 0, 0}};
  const auto tied = SpinWindow({}, tie, 0.0);
  Check(tied.size() == 2 && tied[0].tid == 3 && tied[0].est_ms == 0.0,
        "ties by lower thread id; no TSC rate gives est_ms 0");
  // Slots run out: nullptr and the overflow count.
  SpinCensus full;
  for (size_t i = 0; i < mcla::guest_spin::kMaxThreads; ++i) {
    Check(full.Register(uint32_t(i + 1)) != nullptr, "slots up to kMaxThreads");
  }
  Check(full.Register(9999) == nullptr && full.overflow() == 1 &&
            full.Read().size() == mcla::guest_spin::kMaxThreads,
        "slot overflow is counted, the census keeps the first threads");
}

#if defined(__x86_64__) || defined(_M_X64)
inline uint64_t BenchTsc() { return __rdtsc(); }
inline void BenchPause() { _mm_pause(); }
#else
inline uint64_t BenchTsc() { return 0; }
inline void BenchPause() {}
#endif

// Not a pass/fail timing check: prints what the census adds to one poll (two
// TSC reads, the backoff state and three relaxed counter stores per 32 pause
// hints) and what a thread-local increment per hint would add to a tight
// pause loop (the in-intrinsic alternative).
void TestSpinMicroBenchmark() {
  using Clock = std::chrono::steady_clock;
  constexpr int kPolls = 20000;
  auto ns_per_poll = [&](auto &&body) {
    double best = 1e30;
    for (int round = 0; round < 5; ++round) {
      const auto start = Clock::now();
      for (int i = 0; i < kPolls; ++i) {
        body();
      }
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
      const double ns = double(elapsed) / kPolls;
      best = std::min(best, ns);
    }
    return best;
  };
  auto poll = [] {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 8; ++j) {
        BenchPause();
      }
    }
  };
  static thread_local uint64_t hint_counter = 0;
  auto poll_counted = [] {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 8; ++j) {
        BenchPause();
        ++hint_counter;
      }
    }
  };
  static thread_local mcla::guest_spin::SpinBackoff backoff;
  static mcla::guest_spin::SpinCensus census;
  static thread_local mcla::guest_spin::SpinSlot *slot = census.Register(1);
  auto poll_census = [&] {
    const uint64_t begin = BenchTsc();
    const bool y = backoff.BeforePoll(begin, 0);
    poll();
    const uint64_t end = BenchTsc();
    backoff.AfterPoll(end, y);
    slot->Note(y, end - begin);
  };
  const double base = ns_per_poll(poll);
  const double counted = ns_per_poll(poll_counted);
  const double with_census = ns_per_poll(poll_census);
  std::printf("spin micro-benchmark: 32 pauses %.1f ns/poll, +tls count per hint %.1f ns "
              "(%.2f %%), +census per poll %.1f ns (%.2f %%)\n",
              base, counted, base > 0 ? (counted - base) * 100.0 / base : 0.0, with_census,
              base > 0 ? (with_census - base) * 100.0 / base : 0.0);
  Check(hint_counter > 0 && slot != nullptr && slot->polls.load() > 0,
        "the micro-benchmark ran all three loops");
}

}  // namespace

int main() {
  TestDefaultDecision();
  TestThreadCpuDeltasAndOrder();
  TestThreadCpuTiesAndClockSkew();
  TestNameSanitizing();
  TestJson();
  TestSpinBackoff();
  TestSpinCensusAndLines();
  TestSpinMicroBenchmark();
  std::printf("mcla_perf_split_test: %d checks passed\n", checks);
  return 0;
}
