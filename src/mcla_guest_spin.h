#pragma once

// M11 l100 guest spin census and backoff
// (docs/evidence/M11-l100-sdk-deck-levers-20261114.md, section C).
//
// The PPC yield hint db16cyc is emitted inline by the SDK code generator as
// simde_mm_pause() (rexglue-sdk src/codegen/builders/system.cpp,
// build_db16cyc), so there is no runtime intrinsic to count in. 13 hints
// exist in the generated title code; 8 of them are the GPU-serial poll step
// sub_82412F98 (two nested loops: li r11,4 iterations of 8 db16cyc, i.e. 32
// hints per call, then one read of the GPU read pointer). Its callers loop on
// it while the render thread waits for the GPU (the macXenos b0b75d2b
// observation: one guest thread spins 0.7-1.0 core in D3D fence waits). The
// host already overrides sub_82412F98 (mcla_perf_split.cpp), so the census
// counts its calls there: always on, one TSC read before and after each call
// (two rdtsc per 32 pause hints) and plain per-thread counters.
//
// Backoff (host int mcla_guest_spin_backoff, default 0 = off): after N
// consecutive hints of one wait on a thread, the thread yields the processor
// (SwitchToThread) before the next poll and the count restarts. Polls more
// than kEpisodeGapTicks apart belong to different waits.
//
// MCLA_SPIN v=1 reason=<r> seq=<n> thread=<tid> polls=<n> hints=<n>
//   yields=<n> est_ms=<ms> backoff=<N>      (one line per top thread and
//   split window; est_ms = TSC time inside the polls)
// MCLA_SPIN v=1 reason=<r> seq=<n> thread=none backoff=<N>   (no polls)
//
// Everything here is pure and unit-tested (tests/mcla_perf_split_test.cpp).

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mcla::guest_spin {

inline constexpr uint32_t kHintsPerPoll = 32;
// About 23 us at 2.8 GHz (the Deck TSC), 13 us at 5 GHz.
inline constexpr uint64_t kEpisodeGapTicks = 65536;
inline constexpr size_t kMaxThreads = 64;
inline constexpr size_t kSpinTop = 3;
inline constexpr int32_t kBackoffMax = 1 << 20;

// Per-thread backoff state machine.
struct SpinBackoff {
  uint64_t consecutive_hints = 0;
  uint64_t last_end_tsc = 0;

  // Before a poll at now_tsc with the threshold (0 = off): true when the
  // thread should yield first (at least `threshold` consecutive hints of the
  // current wait already ran).
  bool BeforePoll(uint64_t now_tsc, uint32_t threshold) {
    if (last_end_tsc == 0 || now_tsc < last_end_tsc ||
        now_tsc - last_end_tsc > kEpisodeGapTicks) {
      consecutive_hints = 0;
    }
    return threshold != 0 && consecutive_hints >= threshold;
  }
  // After the poll that ended at end_tsc; a yield before it restarted the
  // count, so only this poll's hints count.
  void AfterPoll(uint64_t end_tsc, bool yielded) {
    consecutive_hints = (yielded ? 0 : consecutive_hints) + kHintsPerPoll;
    last_end_tsc = end_tsc;
  }
};

// One thread's cumulative counters: written by that thread only (relaxed
// load + store), read by the logging thread.
struct SpinSlot {
  std::atomic<uint32_t> tid{0};
  std::atomic<uint64_t> polls{0};
  std::atomic<uint64_t> yields{0};
  std::atomic<uint64_t> ticks{0};

  void Note(bool yielded, uint64_t poll_ticks) {
    polls.store(polls.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    if (yielded) {
      yields.store(yields.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }
    ticks.store(ticks.load(std::memory_order_relaxed) + poll_ticks, std::memory_order_relaxed);
  }
};

struct SpinRow {
  uint32_t tid = 0;
  uint64_t polls = 0;
  uint64_t yields = 0;
  uint64_t ticks = 0;
};

class SpinCensus {
 public:
  // The calling thread's slot (once per thread); nullptr when all slots are
  // taken (counted in overflow()).
  SpinSlot *Register(uint32_t tid) {
    const uint32_t index = next_.fetch_add(1, std::memory_order_relaxed);
    if (index >= kMaxThreads) {
      overflow_.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    slots_[index].tid.store(tid, std::memory_order_release);
    return &slots_[index];
  }
  std::vector<SpinRow> Read() const {
    std::vector<SpinRow> rows;
    const uint32_t count =
        std::min<uint32_t>(next_.load(std::memory_order_acquire), uint32_t(kMaxThreads));
    for (uint32_t i = 0; i < count; ++i) {
      SpinRow row;
      row.tid = slots_[i].tid.load(std::memory_order_acquire);
      if (!row.tid) {
        continue;
      }
      row.polls = slots_[i].polls.load(std::memory_order_relaxed);
      row.yields = slots_[i].yields.load(std::memory_order_relaxed);
      row.ticks = slots_[i].ticks.load(std::memory_order_relaxed);
      rows.push_back(row);
    }
    return rows;
  }
  uint64_t overflow() const { return overflow_.load(std::memory_order_relaxed); }

 private:
  std::array<SpinSlot, kMaxThreads> slots_{};
  std::atomic<uint32_t> next_{0};
  std::atomic<uint64_t> overflow_{0};
};

struct SpinDelta {
  uint32_t tid = 0;
  uint64_t polls = 0;
  uint64_t hints = 0;
  uint64_t yields = 0;
  double est_ms = 0.0;
};

// The window's threads with polls, most hints first (ties: lower tid).
inline std::vector<SpinDelta> SpinWindow(const std::vector<SpinRow> &previous,
                                         const std::vector<SpinRow> &current, double tsc_mhz) {
  std::unordered_map<uint32_t, const SpinRow *> before;
  for (const auto &row : previous) {
    before[row.tid] = &row;
  }
  auto d = [](uint64_t c, uint64_t p) { return c >= p ? c - p : 0; };
  std::vector<SpinDelta> deltas;
  for (const auto &row : current) {
    SpinDelta delta;
    delta.tid = row.tid;
    uint64_t ticks = row.ticks;
    delta.polls = row.polls;
    delta.yields = row.yields;
    if (const auto it = before.find(row.tid); it != before.end()) {
      delta.polls = d(row.polls, it->second->polls);
      delta.yields = d(row.yields, it->second->yields);
      ticks = d(row.ticks, it->second->ticks);
    }
    if (!delta.polls) {
      continue;
    }
    delta.hints = delta.polls * kHintsPerPoll;
    delta.est_ms = tsc_mhz > 0.0 ? double(ticks) / (tsc_mhz * 1000.0) : 0.0;
    deltas.push_back(delta);
  }
  std::sort(deltas.begin(), deltas.end(), [](const SpinDelta &a, const SpinDelta &b) {
    return a.hints != b.hints ? a.hints > b.hints : a.tid < b.tid;
  });
  return deltas;
}

inline std::string SpinReason(std::string_view reason) {
  std::string text;
  for (const char c : reason.substr(0, std::min<size_t>(reason.size(), 24))) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.';
    text.push_back(ok ? c : '_');
  }
  return text.empty() ? std::string("none") : text;
}

// The window's MCLA_SPIN lines (at most `top` threads, or the none line).
inline std::vector<std::string> FormatSpinLines(std::string_view reason, uint64_t seq,
                                                int32_t backoff,
                                                const std::vector<SpinDelta> &deltas,
                                                size_t top = kSpinTop) {
  std::vector<std::string> lines;
  const std::string r = SpinReason(reason);
  char buffer[256];
  if (deltas.empty()) {
    std::snprintf(buffer, sizeof(buffer), "MCLA_SPIN v=1 reason=%s seq=%llu thread=none backoff=%d",
                  r.c_str(), static_cast<unsigned long long>(seq), backoff);
    lines.emplace_back(buffer);
    return lines;
  }
  const size_t count = std::min(top, deltas.size());
  for (size_t i = 0; i < count; ++i) {
    const SpinDelta &s = deltas[i];
    std::snprintf(buffer, sizeof(buffer),
                  "MCLA_SPIN v=1 reason=%s seq=%llu thread=%u polls=%llu hints=%llu yields=%llu "
                  "est_ms=%.2f backoff=%d",
                  r.c_str(), static_cast<unsigned long long>(seq), s.tid,
                  static_cast<unsigned long long>(s.polls),
                  static_cast<unsigned long long>(s.hints),
                  static_cast<unsigned long long>(s.yields), s.est_ms, backoff);
    lines.emplace_back(buffer);
  }
  return lines;
}

constexpr uint32_t BackoffThreshold(int32_t value) {
  return value <= 0 ? 0u : uint32_t(std::min(value, kBackoffMax));
}

}  // namespace mcla::guest_spin
