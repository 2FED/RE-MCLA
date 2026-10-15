#pragma once
// Private diagnostic readiness assertion. Never proves what the owner saw.
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <stop_token>

namespace mcla::owner_ready {
struct Window { uint64_t begin{}, end{}, frequency{}; };
struct Evidence {
  Window window;
  uint64_t deadline{}, opened{}, waiting_before{}, waiting_after{}, ready_before{}, ready_after{};
};
enum class Poll { kEmpty, kSignaled, kFailed };
struct Operations {
  std::function<uint64_t()> now;
  std::function<Poll()> ready;
  std::function<Poll()> waiting;
  std::function<bool()> announce;
  std::function<void()> pause;
};
inline void Require(bool ok, const char* error) { if (!ok) throw std::runtime_error(error); }
inline uint64_t Deadline(Window w) {
  constexpr uint64_t max = INT64_MAX;
  Require(w.frequency >= 10 && w.frequency <= 10000000000ull && w.begin > 0 &&
          w.begin <= max - 180 * w.frequency && w.end == w.begin + 180 * w.frequency,
          "OWNER_READY_INVALID_JOURNAL_WINDOW");
  // Retain sixty seconds for 40s settling, 9.1s collection and cut margin.
  return w.end - 60 * w.frequency;
}
inline Evidence Wait(Window window, std::stop_token stop, const Operations& op) {
  Evidence out{window, Deadline(window)};
  uint64_t previous = window.begin;
  auto clock = [&] {
    Require(!stop.stop_requested(), "OWNER_READY_CANCELLED");
    const auto value = op.now();
    Require(value >= previous && value < out.deadline, "OWNER_READY_CLOCK_OR_DEADLINE");
    previous = value;
    return value;
  };
  out.opened = clock();
  Require(op.ready() == Poll::kEmpty && op.waiting() == Poll::kEmpty,
          "OWNER_READY_OBJECT_NOT_FRESH");
  out.waiting_before = clock();
  Require(op.announce(), "OWNER_READY_ANNOUNCE_FAILED");
  out.waiting_after = clock();
  for (;;) {
    out.ready_before = clock();
    const auto status = op.ready();
    out.ready_after = clock();
    Require(status != Poll::kFailed, "OWNER_READY_POLL_FAILED");
    if (status == Poll::kSignaled) return out;
    op.pause();
  }
}
inline void ValidateSource(const Evidence& e, uint64_t observation) {
  Require(observation >= e.ready_after && observation < e.deadline,
          "OWNER_READY_SOURCE_OUTSIDE_BUDGET");
}
inline void ValidateCut(const Evidence& e, uint64_t cut) {
  Require(cut >= e.ready_after && cut < e.window.end, "OWNER_READY_CUT_OUTSIDE_JOURNAL");
}
} // namespace mcla::owner_ready
