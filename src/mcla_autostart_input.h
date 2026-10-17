#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace mcla::autostart_input {

inline constexpr uint16_t kStartButton = 0x0010;
inline constexpr uint64_t kHoldMilliseconds = 5000;
inline constexpr uint64_t kNeutralAckMilliseconds = 5000;

enum class Status { Idle, HoldingStart, WaitingNeutralAck, Succeeded, Failed };
enum class Error {
  None,
  RepeatedArm,
  ClockRollback,
  DeadlineOverflow,
  MissingDownAck,
  MissingNeutralAck,
  Cancelled,
  ExplicitFailure,
  CounterOverflow,
};

struct Snapshot {
  Status status = Status::Idle;
  Error error = Error::None;
  std::optional<uint64_t> last_ms;
  std::optional<uint64_t> published_ms;
  std::optional<uint64_t> release_deadline_ms;
  std::optional<uint64_t> neutral_ack_deadline_ms;
  // Actual call at which release was observed, not a synthetic deadline time.
  std::optional<uint64_t> released_ms;
  std::optional<uint64_t> down_ack_ms;
  std::optional<uint64_t> neutral_ack_ms;
  uint64_t read_count = 0;
  uint64_t poll_count = 0;
  uint64_t down_read_count = 0;
  uint64_t neutral_read_count = 0;
};

// Pure, one-shot diagnostic input policy. The owner serializes ALL calls and
// snapshot access with one mutex, supplies one monotonic millisecond clock, and
// calls Poll from its bounded timer even when the guest is not reading input.
// Read is the only API returning buttons: it checks time BEFORE returning START.
// No background thread, OS input, reset/rearm, configurable buttons or analog
// channels exist here. The integrating driver must set every analog channel to
// zero and must not cache a prior Read result as a subsequent guest response.
class StartLatch {
 public:
  StartLatch() = default;
  StartLatch(const StartLatch&) = delete;
  StartLatch& operator=(const StartLatch&) = delete;
  StartLatch(StartLatch&&) = delete;
  StartLatch& operator=(StartLatch&&) = delete;

  bool Arm(uint64_t now_ms) noexcept {
    if (!Observe(now_ms)) return false;
    if (state_.status != Status::Idle) {
      SetFailure(Error::RepeatedArm, now_ms);
      return false;
    }
    constexpr auto max_time = std::numeric_limits<uint64_t>::max();
    if (now_ms > max_time - kHoldMilliseconds - kNeutralAckMilliseconds) {
      SetFailure(Error::DeadlineOverflow, now_ms);
      return false;
    }
    state_.published_ms = now_ms;
    state_.release_deadline_ms = now_ms + kHoldMilliseconds;
    state_.neutral_ack_deadline_ms = now_ms + kHoldMilliseconds + kNeutralAckMilliseconds;
    state_.status = Status::HoldingStart;
    return true;
  }

  uint16_t Read(uint64_t now_ms) noexcept {
    if (!Observe(now_ms) || !Increment(state_.read_count, now_ms)) return 0;
    Advance(now_ms);
    if (state_.status == Status::HoldingStart) {
      if (!Increment(state_.down_read_count, now_ms)) return 0;
      if (!state_.down_ack_ms) state_.down_ack_ms = now_ms;
      return kStartButton;
    }
    if (!Increment(state_.neutral_read_count, now_ms)) return 0;
    if (state_.status == Status::WaitingNeutralAck) {
      state_.neutral_ack_ms = now_ms;
      state_.status = Status::Succeeded;
    }
    return 0;
  }

  Status Poll(uint64_t now_ms) noexcept {
    if (Observe(now_ms) && Increment(state_.poll_count, now_ms)) Advance(now_ms);
    return state_.status;
  }

  void Cancel(uint64_t now_ms) noexcept {
    if (Observe(now_ms)) SetFailure(Error::Cancelled, now_ms);
  }

  void Fail(uint64_t now_ms) noexcept {
    if (Observe(now_ms)) SetFailure(Error::ExplicitFailure, now_ms);
  }

  const Snapshot& snapshot() const noexcept { return state_; }

 private:
  bool Observe(uint64_t now_ms) noexcept {
    if (state_.last_ms && now_ms < *state_.last_ms) {
      // Do not manufacture an earlier release time from the invalid clock.
      SetFailure(Error::ClockRollback, *state_.last_ms);
      return false;
    }
    state_.last_ms = now_ms;
    return true;
  }

  bool Increment(uint64_t& counter, uint64_t now_ms) noexcept {
    if (counter == std::numeric_limits<uint64_t>::max()) {
      SetFailure(Error::CounterOverflow, now_ms);
      return false;
    }
    ++counter;
    return true;
  }

  void Advance(uint64_t now_ms) noexcept {
    if (state_.status == Status::HoldingStart && now_ms >= *state_.release_deadline_ms) {
      state_.released_ms = now_ms;
      if (!state_.down_ack_ms) {
        SetFailure(Error::MissingDownAck, now_ms);
        return;
      }
      state_.status = Status::WaitingNeutralAck;
    }
    // Deadline remains anchored to publication, even if a late Poll first
    // observes release. Delayed scheduling must never extend this protocol.
    if (state_.status == Status::WaitingNeutralAck && now_ms >= *state_.neutral_ack_deadline_ms)
      SetFailure(Error::MissingNeutralAck, now_ms);
  }

  void SetFailure(Error error, uint64_t now_ms) noexcept {
    if (state_.status == Status::HoldingStart && !state_.released_ms)
      state_.released_ms = now_ms;
    if (state_.error == Error::None) state_.error = error;
    state_.status = Status::Failed;
  }

  Snapshot state_;
};

}  // namespace mcla::autostart_input
