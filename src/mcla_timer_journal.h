#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

// Native-independent diagnostic storage. Caller supplies every observation;
// there are no clock reads, game-memory reads, callbacks or guest-work locks.
// A runtime seam, process/source binding and lifetime proof remain pending.
namespace mcla::timer_journal {
inline constexpr uint64_t kMaxQpc = uint64_t(std::numeric_limits<int64_t>::max());
inline constexpr uint64_t kMaxFrequency = 10000000000ull;
inline constexpr uint64_t kMaxWindowSeconds = 12;
inline constexpr bool kSimulationCorrectnessProven = false;
inline constexpr bool kFpsProven = false;
inline constexpr bool kNativeAdmitted = false;

struct Window {
  uint64_t begin_qpc = 0;
  uint64_t end_qpc = 0;  // Exclusive end; touching it is not wholly interior.
  uint64_t qpc_frequency = 0;
  constexpr bool valid() const noexcept {
    return begin_qpc && begin_qpc < end_qpc && end_qpc <= kMaxQpc &&
           qpc_frequency && qpc_frequency <= kMaxFrequency &&
           end_qpc - begin_qpc <= kMaxWindowSeconds * qpc_frequency;
  }
};

struct Identity {
  uint32_t timer_address = 0;
  uint32_t caller_address = 0;  // Opaque incoming guest return/caller address.
  uint64_t thread_identity = 0;  // Caller must document its identity domain.
  // An address is NOT proof of an object lifetime or a simulation consumer.
  constexpr bool valid() const noexcept {
    return timer_address && caller_address && thread_identity;
  }
};

// Exact opaque fields from the audited timer path. No interpretation of reset,
// modes, histories, floating values or sums is performed by this core.
inline constexpr std::array<uint32_t, 18> kWordOffsets = {
    8, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 60, 84, 88, 92, 96, 100, 104};
inline constexpr std::array<uint32_t, 3> kByteOffsets = {56, 58, 72};
inline constexpr std::array<uint32_t, 1> kQwordOffsets = {64};
struct State {
  std::array<uint32_t, kWordOffsets.size()> words{};
  std::array<uint8_t, kByteOffsets.size()> bytes{};
  std::array<uint64_t, kQwordOffsets.size()> qwords{};
  constexpr bool operator==(const State&) const = default;
};

struct Observation {
  Identity identity;
  uint64_t entry_qpc = 0;
  uint64_t exit_qpc = 0;
  uint8_t incoming_r5_low_byte = 0;
  uint64_t incoming_f1_bits = 0;
  // Availability is a caller claim, NOT proof of coherent live memory reads.
  State before;
  State after;
  bool before_available = false;
  bool after_available = false;
};

enum Issue : uint32_t {
  kNone = 0,
  kInvalidWindow = 1u << 0,
  kInvalidClock = 1u << 1,
  kMissingIdentity = 1u << 2,
  kMissingState = 1u << 3,
  kStraddlesBegin = 1u << 4,
  kStraddlesEnd = 1u << 5,
  kOutsideBefore = 1u << 6,
  kOutsideAfter = 1u << 7,
  kWitnessIntervalsOverlapOrTouch = 1u << 8,
  kSharedAddressOverlapOrTouch = 1u << 9,
  kEntryOrderRegressed = 1u << 10,
  kExitOrderRegressed = 1u << 11,
  kStorageOverflow = 1u << 12,
  kCounterSaturated = 1u << 13,
};

struct Record {
  Observation observation;
  uint64_t observed_ordinal = 0;  // Append order, not inferred execution order.
  uint32_t issues = kNone;
  bool wholly_inside_window = false;
};

template<size_t Capacity = 1024>
class Journal {
  static_assert(Capacity > 0 && Capacity <= 4096);
 public:
  explicit constexpr Journal(Window window) noexcept : window_(window) {
    if (!window.valid()) issues_ |= kInvalidWindow;
  }

  // Not thread-safe: an integration may synchronize short journal publication,
  // but MUST NOT hold that synchronization across original guest work. No
  // wrapper is provided; original-once/context/lifecycle proof is still pending.
  // Overflow preserves existing records, rejects the new append, and is sticky.
  bool observe(const Observation& input) noexcept {
    if (observed_calls_ != std::numeric_limits<uint64_t>::max()) {
      ++observed_calls_;
    } else {
      issues_ |= kCounterSaturated;
    }
    if (size_ == Capacity) {
      issues_ |= kStorageOverflow;
      return false;
    }
    Record row{input, observed_calls_};
    const bool clock_valid = valid_clock(input);
    if (!window_.valid()) row.issues |= kInvalidWindow;
    if (!clock_valid) row.issues |= kInvalidClock;
    if (!input.identity.valid()) row.issues |= kMissingIdentity;
    if (!input.before_available || !input.after_available) row.issues |= kMissingState;
    if (clock_valid && window_.valid()) {
      if (input.exit_qpc < window_.begin_qpc) row.issues |= kOutsideBefore;
      if (input.entry_qpc >= window_.end_qpc) row.issues |= kOutsideAfter;
      if (input.entry_qpc < window_.begin_qpc && input.exit_qpc >= window_.begin_qpc)
        row.issues |= kStraddlesBegin;
      if (input.entry_qpc < window_.end_qpc && input.exit_qpc >= window_.end_qpc)
        row.issues |= kStraddlesEnd;
      row.wholly_inside_window = input.entry_qpc >= window_.begin_qpc &&
                                 input.exit_qpc < window_.end_qpc;
    }
    // Closed witness intervals are conservative: equal ticks cannot prove
    // ordering. Overlap here is not proof of simultaneous original guest work.
    for (size_t i = 0; clock_valid && i < size_; ++i) {
      auto& old = records_[i];
      if (!valid_clock(old.observation)) continue;
      const auto& previous = old.observation;
      if (input.entry_qpc <= previous.exit_qpc && previous.entry_qpc <= input.exit_qpc) {
        uint32_t overlap = kWitnessIntervalsOverlapOrTouch;
        if (input.identity.timer_address &&
            input.identity.timer_address == previous.identity.timer_address)
          overlap |= kSharedAddressOverlapOrTouch;
        row.issues |= overlap;
        old.issues |= overlap;
      }
    }
    if (clock_valid && have_previous_clock_) {
      if (input.entry_qpc < previous_entry_) row.issues |= kEntryOrderRegressed;
      if (input.exit_qpc < previous_exit_) row.issues |= kExitOrderRegressed;
    }
    if (clock_valid) {
      previous_entry_ = input.entry_qpc;
      previous_exit_ = input.exit_qpc;
      have_previous_clock_ = true;
    }
    issues_ |= row.issues;
    records_[size_++] = row;
    return true;
  }

  constexpr Window window() const noexcept { return window_; }
  // Borrowed view, not a frozen snapshot: observe() may update prior overlap
  // flags. Readers and writers require the same external synchronization.
  constexpr std::span<const Record> records() const noexcept {
    return {records_.data(), size_};
  }
  constexpr uint64_t observed_calls() const noexcept { return observed_calls_; }
  constexpr uint32_t issues() const noexcept { return issues_; }
  constexpr bool overflowed() const noexcept { return (issues_ & kStorageOverflow) != 0; }

 private:
  static constexpr bool valid_clock(const Observation& input) noexcept {
    return input.entry_qpc && input.entry_qpc <= input.exit_qpc && input.exit_qpc <= kMaxQpc;
  }
  Window window_;
  std::array<Record, Capacity> records_{};
  size_t size_ = 0;
  uint64_t observed_calls_ = 0;
  uint32_t issues_ = kNone;
  uint64_t previous_entry_ = 0, previous_exit_ = 0;
  bool have_previous_clock_ = false;
};
}  // namespace mcla::timer_journal
